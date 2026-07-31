// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_manager.h"

#include <shlobj.h>
#include <stddef.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/limited_access_features.h"
#include "base/win/post_async_results.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "taskbar_manager.h"
#include "windows.ui.shell.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::UI::Shell::IID_ITaskbarManagerStatics;
using ABI::Windows::UI::Shell::ITaskbarManager;
using ABI::Windows::UI::Shell::ITaskbarManagerStatics;
using Microsoft::WRL::ComPtr;

using browser_util::PinResultCallback;
using content::BrowserThread;

namespace browser_util {

namespace {

using ResultMetricCallback = base::OnceCallback<void(PinResultMetric)>;

constexpr const char* kShouldPinToTaskbarResultHistogram =
    "Windows.ShouldPinToTaskbarResult";
constexpr const char* kTaskbarPinResultHistogram = "Windows.TaskbarPinResult";

// LINT.IfChange(PinAppToTaskbarChannel)
// These must be kept in sync with the enum in taskbar_manager.h as well as the
// variants list in /tools/metrics/histograms/metadata/windows/histograms.xml.
constexpr std::array kChannels = {
    "DefaultBrowserInfoBar",
    "PinToTaskbarInfoBar",
    "FirstRunExperience",
    "SettingsPage",
    "PinWebApp",
    "DefaultBrowserBubbleDialog",
    "DefaultBrowserModalDialogWithSettingsImage",
    "DefaultBrowserModalDialogWithoutSettingsImage",
    "DefaultBrowserVisualGuidedSetter",
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/histograms.xml:PinAppToTaskbarChannel)

// Every taskbar pin flow runs on this sequence, and in particular every update
// of the process-wide App User Model ID (AUMI) happens here.
// `::SetCurrentProcessExplicitAppUserModelID()` frees and replaces a string
// owned by the shell without any internal synchronization, so overlapping
// calls from two threads corrupt the process heap. Confining every update to a
// single sequence makes them mutually exclusive.
//
// This sequence also runs the `ITaskbarManager` eligibility getters, which
// issue synchronous, blocking cross-process RPCs to the shell (explorer.exe)
// and therefore must not run on the UI thread.
base::LazyThreadPoolSequencedTaskRunner g_pin_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE));

// A pin flow waiting for the process App User Model ID to become available.
struct QueuedPinFlow {
  std::wstring app_user_model_id;
  internal::PinFlowCallback flow;
};

// Whether a pin flow currently owns the process App User Model ID. Only
// accessed on the pin sequence.
bool g_app_user_model_id_owned = false;

// Flows waiting for the process App User Model ID. Only accessed on the pin
// sequence.
base::circular_deque<QueuedPinFlow>& GetQueuedPinFlows() {
  static base::NoDestructor<base::circular_deque<QueuedPinFlow>> queued_flows;
  return *queued_flows;
}

// The one and only place that updates the process-wide App User Model ID. See
// `g_pin_task_runner` for why this is restricted to the pin sequence.
void SetProcessAppUserModelId(const std::wstring& app_user_model_id) {
  CHECK(g_pin_task_runner.Get()->RunsTasksInCurrentSequence());
  ::SetCurrentProcessExplicitAppUserModelID(app_user_model_id.c_str());
}

void ReleaseProcessAppUserModelId();

// Gives `flow` exclusive ownership of the process App User Model ID and runs
// it.
void StartPinFlow(const std::wstring& app_user_model_id,
                  internal::PinFlowCallback flow) {
  CHECK(g_pin_task_runner.Get()->RunsTasksInCurrentSequence());
  CHECK(!g_app_user_model_id_owned);
  g_app_user_model_id_owned = true;

  // Chrome doesn't otherwise set a process App User Model ID, so it is cleared
  // again once the flow is done. ITaskbarManager requires it to match the app
  // requesting the pin, and `get_IsPinningAllowed()` only returns true when
  // there is a Start Menu shortcut with this ID, so it must be set before the
  // eligibility query and stay set for the rest of the flow.
  SetProcessAppUserModelId(app_user_model_id);

  // `base::BindPostTask()` makes the release hop back to the pin sequence, so
  // that the App User Model ID is still only ever updated there no matter which
  // thread ends up destroying the runner.
  std::move(flow).Run(base::ScopedClosureRunner(base::BindPostTask(
      g_pin_task_runner.Get(), base::BindOnce(&ReleaseProcessAppUserModelId))));
}

// Clears the process App User Model ID and hands it to the next queued flow, if
// any.
void ReleaseProcessAppUserModelId() {
  CHECK(g_pin_task_runner.Get()->RunsTasksInCurrentSequence());
  CHECK(g_app_user_model_id_owned);

  SetProcessAppUserModelId(std::wstring());
  g_app_user_model_id_owned = false;

  if (GetQueuedPinFlows().empty()) {
    return;
  }
  QueuedPinFlow next = std::move(GetQueuedPinFlows().front());
  GetQueuedPinFlows().pop_front();
  StartPinFlow(next.app_user_model_id, std::move(next.flow));
}

// Returns whether pinning is allowed or not. If it returns std::nullopt, an
// ITaskbarManager method returned an error.
std::optional<bool> IsPinningAllowed(
    const ComPtr<ITaskbarManager>& taskbar_manager) {
  // `get_IsSupported()` and `get_IsPinningAllowed()` issue synchronous,
  // blocking cross-process RPCs to the shell (explorer.exe), so they must not
  // run on the UI thread. The TaskbarManager WinRT object is agile
  // (MarshalingBehavior=Agile, ThreadingModel=Both), so these queries are safe
  // to call from the pin sequence, which is also where the object was created.
  // Only `RequestPinCurrentAppAsync()`, which displays a confirmation dialog,
  // requires the UI thread.
  CHECK(g_pin_task_runner.Get()->RunsTasksInCurrentSequence());
  boolean supported;
  HRESULT hr = taskbar_manager->get_IsSupported(&supported);
  if (FAILED(hr)) {
    return std::nullopt;
  }
  if (!supported) {
    return false;
  }
  boolean allowed = false;
  hr = taskbar_manager->get_IsPinningAllowed(&allowed);
  if (FAILED(hr)) {
    return std::nullopt;
  }
  return allowed;
}

void PinnedRequestResult(ComPtr<ITaskbarManager> taskbar_manager,
                         ResultMetricCallback callback,
                         boolean pin_request_result) {
  std::move(callback).Run(pin_request_result
                              ? PinResultMetric::kSuccess
                              : PinResultMetric::kPinCurrentAppFailed);
}

// This helper splits `callback` three ways for use with `PostAsyncHandlers`,
// which has three separate paths to outcomes: invoke a success callback, invoke
// an error callback, or return an error.
template <typename... Args>
std::tuple<base::OnceCallback<void(Args...)>,
           base::OnceCallback<void(Args...)>,
           base::OnceCallback<void(Args...)>>
SplitOnceCallbackIntoThree(base::OnceCallback<void(Args...)> callback) {
  auto first_split = base::SplitOnceCallback(std::move(callback));
  auto second_split = base::SplitOnceCallback(std::move(first_split.first));
  return {std::move(first_split.second), std::move(second_split.first),
          std::move(second_split.second)};
}

void OnIsCurrentAppPinnedResult(ComPtr<ITaskbarManager> taskbar_manager,
                                bool check_only,
                                ResultMetricCallback callback,
                                boolean is_current_app_pinned) {
  if (is_current_app_pinned) {
    std::move(callback).Run(PinResultMetric::kAlreadyPinned);
    return;
  }
  if (check_only) {
    // If asking if Chrome should offer to pin, the answer is yes.
    std::move(callback).Run(PinResultMetric::kSuccess);
    return;
  }
  ComPtr<IAsyncOperation<bool>> request_pin_operation = nullptr;
  HRESULT hr =
      taskbar_manager->RequestPinCurrentAppAsync(&request_pin_operation);
  if (FAILED(hr)) {
    std::move(callback).Run(PinResultMetric::kTaskbarManagerError);
    return;
  }

  auto split_callback = SplitOnceCallbackIntoThree(std::move(callback));

  hr = base::win::PostAsyncHandlers(
      request_pin_operation.Get(),
      base::BindOnce(&PinnedRequestResult, std::move(taskbar_manager),
                     std::move(std::get<0>(split_callback))),
      base::BindOnce(
          [](base::OnceCallback<void(PinResultMetric)> pin_callback) {
            std::move(pin_callback)
                .Run(PinResultMetric::kPostAsyncResultsFailed);
          },
          std::move(std::get<1>(split_callback))));
  if (FAILED(hr)) {
    std::move(std::get<2>(split_callback))
        .Run(PinResultMetric::kPostAsyncResultsFailed);
  }
}

// Checks whether the current app is already pinned and, if not (and
// `check_only` is false), requests that it be pinned. This runs on the UI
// thread because `RequestPinCurrentAppAsync()` displays a confirmation dialog.
// Pinning eligibility has already been verified on the pin sequence by the
// caller, which also still owns the process App User Model ID.
void PinCurrentAppOnUIThread(
    ComPtr<ITaskbarManager> taskbar_manager,
    bool check_only,
    base::OnceCallback<void(PinResultMetric)> callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  ComPtr<IAsyncOperation<bool>> is_pinned_operation = nullptr;
  HRESULT hr = taskbar_manager->IsCurrentAppPinnedAsync(&is_pinned_operation);
  if (FAILED(hr)) {
    std::move(callback).Run(PinResultMetric::kTaskbarManagerError);
    return;
  }
  auto split_callback = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      is_pinned_operation.Get(),
      base::BindOnce(&OnIsCurrentAppPinnedResult, std::move(taskbar_manager),
                     check_only, std::move(std::get<0>(split_callback))),
      base::BindOnce(
          [](base::OnceCallback<void(PinResultMetric)> pin_callback) {
            std::move(pin_callback)
                .Run(PinResultMetric::kPostAsyncResultsFailed);
          },
          std::move(std::get<1>(split_callback))));
  if (FAILED(hr)) {
    std::move(std::get<2>(split_callback))
        .Run(PinResultMetric::kPostAsyncResultsFailed);
  }
}

// Attempts to pin a shortcut with the process App User Model ID to the taskbar.
// Runs on the pin sequence, where the process App User Model ID is already set;
// `release_app_user_model_id` keeps it set until this flow completes.
//
// The early-out paths below run `callback` from the pin sequence. That is safe:
// `ShouldOfferToPin()` and `PinAppToTaskbar()` bind the caller's callback with
// `base::BindPostTaskToCurrentDefault()`, so it is still delivered on the
// sequence that requested the pin, and the metrics recorded in between are
// thread-safe.
void PinWithLimitedAccessFeature(
    bool check_only,
    ResultMetricCallback callback,
    base::ScopedClosureRunner release_app_user_model_id) {
  CHECK(g_pin_task_runner.Get()->RunsTasksInCurrentSequence());
  base::win::AssertComInitialized();

  // Bind the release into `callback` so that the process App User Model ID
  // stays set until the flow produces a result, and is released even if the
  // callback chain is dropped without ever running.
  callback = base::BindOnce(
      [](base::ScopedClosureRunner release, ResultMetricCallback callback,
         PinResultMetric result) { std::move(callback).Run(result); },
      std::move(release_app_user_model_id), std::move(callback));

  ComPtr<IInspectable> taskbar_statics_inspectable;

  HRESULT hr = base::win::RoGetActivationFactory(
      base::win::HStringReference(RuntimeClass_Windows_UI_Shell_TaskbarManager)
          .Get(),
      IID_ITaskbarManagerStatics, &taskbar_statics_inspectable);
  if (FAILED(hr)) {
    std::move(callback).Run(PinResultMetric::kCOMError);
    return;
  }

  ComPtr<ITaskbarManagerStatics> taskbar_statics;

  hr = taskbar_statics_inspectable.As(&taskbar_statics);
  if (FAILED(hr)) {
    std::move(callback).Run(PinResultMetric::kCOMError);
    return;
  }

  ComPtr<ITaskbarManager> taskbar_manager;

  hr = taskbar_statics->GetDefault(&taskbar_manager);
  if (FAILED(hr)) {
    std::move(callback).Run(PinResultMetric::kCOMError);
    return;
  }

  // There must be a shortcut with the process App User Model ID in the start
  // menu for this to return true. This is a blocking shell RPC, which is why it
  // runs here rather than on the UI thread.
  std::optional<bool> is_pinning_allowed = IsPinningAllowed(taskbar_manager);
  if (!is_pinning_allowed.has_value()) {
    std::move(callback).Run(PinResultMetric::kTaskbarManagerError);
    return;
  }
  if (!*is_pinning_allowed) {
    std::move(callback).Run(PinResultMetric::kPinningNotAllowed);
    return;
  }

  // The remaining work uses the WinRT async machinery and, for
  // `RequestPinCurrentAppAsync()`, displays a confirmation dialog, so it runs
  // on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PinCurrentAppOnUIThread, std::move(taskbar_manager),
                     check_only, std::move(callback)));
}

void PinAppToTaskbarInternal(const std::wstring& app_user_model_id,
                             PinAppToTaskbarChannel channel,
                             bool check_only,
                             PinResultCallback callback) {
  // Do the initial work, which does a lot of COM stuff and issues blocking
  // shell RPCs, on the pin sequence.
  if (!g_pin_task_runner.Get()->RunsTasksInCurrentSequence()) {
    g_pin_task_runner.Get()->PostTask(
        FROM_HERE, base::BindOnce(&PinAppToTaskbarInternal, app_user_model_id,
                                  channel, check_only, std::move(callback)));
    return;
  }

  // Wrap `callback` in a separate closure to record detailed success and
  // failure metrics.
  ResultMetricCallback pin_result_callback(base::BindOnce(
      [](PinResultCallback pin_callback, PinAppToTaskbarChannel channel,
         bool check_only, PinResultMetric result) {
        base::UmaHistogramEnumeration(check_only
                                          ? kShouldPinToTaskbarResultHistogram
                                          : kTaskbarPinResultHistogram,
                                      result);
        base::UmaHistogramEnumeration(
            check_only ? base::StrCat({kShouldPinToTaskbarResultHistogram, ".",
                                       kChannels[static_cast<int>(channel)]})
                       : base::StrCat({kTaskbarPinResultHistogram, ".",
                                       kChannels[static_cast<int>(channel)]}),
            result);
        std::move(pin_callback).Run(result == PinResultMetric::kSuccess);
      },
      std::move(callback), channel, check_only));

  if (!PinLimitedAccessFeatureAvailable()) {
    std::move(pin_result_callback).Run(PinResultMetric::kFeatureNotAvailable);
    return;
  }

  // The rest of the flow needs the process App User Model ID set to
  // `app_user_model_id`, and only one flow may own it at a time.
  internal::RunWithProcessAppUserModelId(
      app_user_model_id,
      base::BindOnce(&PinWithLimitedAccessFeature, check_only,
                     std::move(pin_result_callback)));
}

}  // namespace

bool PinLimitedAccessFeatureAvailable() {
  static constexpr wchar_t taskbar_api_token[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      L"InBNYixzyiUzivxj5T/HqA==";
#else
      L"ILzQYl3daXqTIyjmNj5xwg==";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::win::TryToUnlockLimitedAccessFeature(
      L"com.microsoft.windows.taskbar.pin", taskbar_api_token);
}

void ShouldOfferToPin(const std::wstring& app_user_model_id,
                      PinAppToTaskbarChannel channel,
                      PinResultCallback callback) {
  auto callback_on_current_thread =
      base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE);

  PinAppToTaskbarInternal(app_user_model_id, channel, /*check_only=*/true,
                          std::move(callback_on_current_thread));
}

void PinAppToTaskbar(const std::wstring& app_user_model_id,
                     PinAppToTaskbarChannel channel,
                     PinResultCallback callback) {
  auto callback_on_current_thread =
      base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE);

  PinAppToTaskbarInternal(app_user_model_id, channel, /*check_only=*/false,
                          std::move(callback_on_current_thread));
}

namespace internal {

void RunWithProcessAppUserModelId(const std::wstring& app_user_model_id,
                                  PinFlowCallback flow) {
  if (!g_pin_task_runner.Get()->RunsTasksInCurrentSequence()) {
    g_pin_task_runner.Get()->PostTask(
        FROM_HERE, base::BindOnce(&RunWithProcessAppUserModelId,
                                  app_user_model_id, std::move(flow)));
    return;
  }

  // Another flow still owns the process App User Model ID. Wait for it rather
  // than overwriting the ID it is using.
  if (g_app_user_model_id_owned) {
    GetQueuedPinFlows().push_back({app_user_model_id, std::move(flow)});
    return;
  }

  StartPinFlow(app_user_model_id, std::move(flow));
}

}  // namespace internal

}  // namespace browser_util
