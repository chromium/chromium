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

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
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
    "DefaultBrowserInfoBar", "PinToTaskbarInfoBar", "FirstRunExperience",
    "SettingsPage",          "PinWebApp",
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/histograms.xml:PinAppToTaskbarChannel)

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

// Returns whether pinning is allowed or not. If it returns std::nullopt, an
// ITaskbarManager method returned an error.
std::optional<bool> IsPinningAllowed(
    const ComPtr<ITaskbarManager>& taskbar_manager) {
  // Windows requires that this is run on the UI thread.
  CHECK_CURRENTLY_ON(BrowserThread::UI);
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
  ::SetCurrentProcessExplicitAppUserModelID(L"");
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

void PinToTaskbarIfAllowedOnUIThread(
    ComPtr<ITaskbarManager> taskbar_manager,
    const std::wstring& app_user_model_id,
    bool check_only,
    base::OnceCallback<void(PinResultMetric)> callback) {
  // Chrome doesn't currently set this, so it will be cleared when the
  // pinning process is done. ITaskbarManager requires that this be set to the
  // same value as the window requesting the pinning.
  ::SetCurrentProcessExplicitAppUserModelID(app_user_model_id.c_str());

  // There must be a shortcut with AUMI `app_user_model_id` in the start menu
  // for this to return true.
  auto is_pinning_allowed = IsPinningAllowed(taskbar_manager);
  if (!is_pinning_allowed.has_value()) {
    std::move(callback).Run(PinResultMetric::kTaskbarManagerError);
    return;
  }
  if (!*is_pinning_allowed) {
    std::move(callback).Run(PinResultMetric::kPinningNotAllowed);
    return;
  }

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

// Attempts to pin a shortcut with AUMI `app_user_model_id` to the taskbar.
// Pinning is done on the UI thread, asynchronously.
void PinWithLimitedAccessFeature(const std::wstring& app_user_model_id,
                                 bool check_only,
                                 ResultMetricCallback callback) {
  base::win::AssertComInitialized();

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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PinToTaskbarIfAllowedOnUIThread, taskbar_manager,
                     app_user_model_id, check_only, std::move(callback)));
}

void PinAppToTaskbarInternal(const std::wstring& app_user_model_id,
                             PinAppToTaskbarChannel channel,
                             bool check_only,
                             PinResultCallback callback) {
  // Do the initial work, which does a lot of COM stuff, on a background thread.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(&PinAppToTaskbarInternal, app_user_model_id,
                                  channel, check_only, std::move(callback)));
    return;
  }

  // Wrap `callback` in a separate closure to make sure the current process's
  // App User Model Id is cleared, and to record detailed success and failure
  // metrics.
  ResultMetricCallback pin_result_callback(base::BindOnce(
      [](PinResultCallback pin_callback, PinAppToTaskbarChannel channel,
         bool check_only, PinResultMetric result) {
        ::SetCurrentProcessExplicitAppUserModelID(L"");
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

  if (PinLimitedAccessFeatureAvailable()) {
    PinWithLimitedAccessFeature(app_user_model_id, check_only,
                                std::move(pin_result_callback));
  } else {
    std::move(pin_result_callback).Run(PinResultMetric::kFeatureNotAvailable);
  }
}

}  // namespace

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

}  // namespace browser_util
