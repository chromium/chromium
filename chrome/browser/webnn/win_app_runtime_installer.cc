// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webnn/win_app_runtime_installer.h"

#include <windows.h>

#include <Windows.ApplicationModel.store.preview.installcontrol.h>
#include <appmodel.h>
#include <wrl.h>

#include <ranges>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/webnn/webnn_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/public/mojom/features.mojom-features.h"

namespace webnn {

namespace {

namespace abi_install =
    ABI::Windows::ApplicationModel::Store::Preview::InstallControl;

using AppInstallAsyncOp =
    __FIAsyncOperation_1___FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;
using AppInstallStatusChangedHandler =
    __FITypedEventHandler_2_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem_IInspectable;
using AppInstallItems =
    __FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;

// The product ID of the Windows App Runtime package in Microsoft Store.
constexpr std::wstring_view kWinAppRuntimeProductId = L"9NKRJ3SJ9SDG";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WinAppRuntimeInstallStateUma)
enum class WinAppRuntimeInstallStateUma {
  kWindowsVersionTooOld = 0,
  kActivationFailure = 1,
  kCompleted = 2,
  kError = 3,
  kCanceled = 4,
  kPaused = 5,
  kPausedLowBattery = 6,
  kPausedWiFiRecommended = 7,
  kPausedWiFiRequired = 8,
  kRuntimeAlreadyPresent = 9,
  kInstallationFailedToStart = 10,

  kMaxValue = kInstallationFailedToStart,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:WinAppRuntimeInstallStateUma)

void RecordInstallState(WinAppRuntimeInstallStateUma state) {
  base::UmaHistogramEnumeration("WebNN.ORT.WinAppRuntimeInstallState", state);
}

// Creates a Windows App Runtime package dependency with the lifetime of the
// user data directory.
std::wstring TryCreateWinAppRuntimePackageDependency() {
  base::FilePath user_data_dir =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA);

  return TryCreatePackageDependencyForFilePath(kWinAppRuntimePackageFamilyName,
                                               kWinAppRuntimePackageMinVersion,
                                               user_data_dir);
}

// Called if the installation succeeds (or already installed) and the package
// dependency is created successfully. Updates the local state prefs with the
// package information and provided dependency ID.
void UpdatePrefs(const std::wstring& dependency_id) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kWinAppRuntimePackageDependencyId,
                         base::WideToUTF8(dependency_id));
  local_state->SetString(prefs::kWinAppRuntimePackageFamilyName,
                         base::WideToUTF8(kWinAppRuntimePackageFamilyName));
  local_state->SetString(prefs::kWinAppRuntimePackageMinVersion,
                         kWinAppRuntimePackageMinVersionString);
}

// Called after all `AppInstallItems` reach the complete state (succeeded,
// canceled or failed). `app_install_manager` is kept alive by this function to
// ensure the registered status change callbacks will be invoked.
void OnInstallationCompleted(
    Microsoft::WRL::ComPtr<
        abi_install::IAppInstallManager> /*app_install_manager*/,
    std::vector<bool> results) {
  if (std::ranges::any_of(results, [](bool success) { return !success; })) {
    return;
  }

  std::wstring dependency_id = TryCreateWinAppRuntimePackageDependency();
  if (dependency_id.empty()) {
    return;
  }

  UpdatePrefs(dependency_id);
}

// Called after `StartProductInstallAsync()` completes. Adds a callback for each
// `AppInstallItem` to track their status change.
void OnInstallationStarted(
    Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager,
    Microsoft::WRL::ComPtr<AppInstallItems> items) {
  uint32_t count = 0;
  HRESULT hr = items->get_Size(&count);
  CHECK_EQ(hr, S_OK);
  if (count == 0) {
    RecordInstallState(
        WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
    return;
  }

  // Wait for all the app install items to complete.
  base::ConcurrentCallbacks<bool> concurrent_callbacks;

  for (uint32_t i = 0; i < count; ++i) {
    Microsoft::WRL::ComPtr<abi_install::IAppInstallItem> item;
    hr = items->GetAt(i, &item);
    CHECK_EQ(hr, S_OK);

    // `token` receives the value assigned by `add_StatusChanged()` below.
    // It is used to unregister the status change event handler.
    auto token = std::make_unique<EventRegistrationToken>();
    auto* token_ptr = token.get();

    // Register the status change event handler for `item`.
    item->add_StatusChanged(
        Microsoft::WRL::Callback<AppInstallStatusChangedHandler>(
            [token = std::move(token),
             callback = base::BindPostTaskToCurrentDefault(
                 concurrent_callbacks.CreateCallback())](
                abi_install::IAppInstallItem* item,
                IInspectable* args) mutable {
              Microsoft::WRL::ComPtr<abi_install::IAppInstallStatus> status;
              HRESULT hr = item->GetCurrentStatus(&status);
              CHECK_EQ(hr, S_OK);

              abi_install::AppInstallState state;
              hr = status->get_InstallState(&state);
              CHECK_EQ(hr, S_OK);

              switch (state) {
                case abi_install::AppInstallState::AppInstallState_Completed: {
                  RecordInstallState(WinAppRuntimeInstallStateUma::kCompleted);
                  item->remove_StatusChanged(*token);
                  if (callback) {
                    std::move(callback).Run(true);
                  }
                  break;
                }
                case abi_install::AppInstallState::AppInstallState_Error: {
                  RecordInstallState(WinAppRuntimeInstallStateUma::kError);
                  item->remove_StatusChanged(*token);
                  if (callback) {
                    std::move(callback).Run(false);
                  }
                  break;
                }
                case abi_install::AppInstallState::AppInstallState_Canceled: {
                  RecordInstallState(WinAppRuntimeInstallStateUma::kCanceled);
                  item->remove_StatusChanged(*token);
                  if (callback) {
                    std::move(callback).Run(false);
                  }
                  break;
                }
                case abi_install::AppInstallState::AppInstallState_Paused: {
                  RecordInstallState(WinAppRuntimeInstallStateUma::kPaused);
                  break;
                }
                case abi_install::AppInstallState::
                    AppInstallState_PausedLowBattery: {
                  RecordInstallState(
                      WinAppRuntimeInstallStateUma::kPausedLowBattery);
                  break;
                }
                case abi_install::AppInstallState::
                    AppInstallState_PausedWiFiRecommended: {
                  RecordInstallState(
                      WinAppRuntimeInstallStateUma::kPausedWiFiRecommended);
                  break;
                }
                case abi_install::AppInstallState::
                    AppInstallState_PausedWiFiRequired: {
                  RecordInstallState(
                      WinAppRuntimeInstallStateUma::kPausedWiFiRequired);
                  break;
                }
                default:
                  break;
              }
              return S_OK;
            })
            .Get(),
        token_ptr);
  }

  std::move(concurrent_callbacks)
      .Done(base::BindOnce(&OnInstallationCompleted,
                           std::move(app_install_manager)));
}

// Activates and returns the IAppInstallManager instance.
Microsoft::WRL::ComPtr<abi_install::IAppInstallManager>
ActivateAppInstallManager() {
  // `RoActivateInstance()` below loads Microsoft Store Install Service dlls.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // `app_install_manager` must remain valid throughout the entire asynchronous
  // installation process, otherwise WinRT may release related resources
  // prematurely, causing the callbacks not to be triggered.
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager;
  HRESULT hr = base::win::RoActivateInstance(
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_ApplicationModel_Store_Preview_InstallControl_AppInstallManager)
          .get(),
      &app_install_manager);
  if (FAILED(hr)) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kActivationFailure);
    return nullptr;
  }
  return app_install_manager;
}

// Starts the installation using the IAppInstallManager API.
void StartInstallation(Microsoft::WRL::ComPtr<abi_install::IAppInstallManager>
                           app_install_manager) {
  if (!app_install_manager) {
    return;
  }

  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager3>
      app_install_manager_3;
  HRESULT hr = app_install_manager.As(&app_install_manager_3);
  CHECK_EQ(hr, S_OK);

  auto catalog_id = base::win::ScopedHString::Create(std::wstring_view());
  auto flight_id = base::win::ScopedHString::Create(std::wstring_view());
  auto client_id = base::win::ScopedHString::Create(std::wstring_view());
  auto correlation_vector =
      base::win::ScopedHString::Create(std::wstring_view());

  Microsoft::WRL::ComPtr<AppInstallAsyncOp> async_op;
  hr = app_install_manager_3->StartProductInstallAsync(
      base::win::ScopedHString::Create(kWinAppRuntimeProductId).get(),
      catalog_id.get(), flight_id.get(), client_id.get(),
      /*repair=*/false, /*forceUseOfNonRemovableStorage=*/false,
      correlation_vector.get(), /*targetVolume=*/nullptr, &async_op);
  if (FAILED(hr)) {
    RecordInstallState(
        WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
    return;
  }

  hr = base::win::PostAsyncHandlers(
      async_op.Get(),
      base::BindOnce(&OnInstallationStarted, std::move(app_install_manager)),
      base::BindOnce([](HRESULT /*hr*/) {
        RecordInstallState(
            WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
      }));
  if (FAILED(hr)) {
    RecordInstallState(
        WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
  }
}

// Ensures the Windows App Runtime package is installed and up to date.
// Runs on browser's UI thread.
void EnsureInstallation() {
  PrefService* local_state = g_browser_process->local_state();
  const std::string& dependency_id =
      local_state->GetString(prefs::kWinAppRuntimePackageDependencyId);

  if (!dependency_id.empty()) {
    const std::string& family_name =
        local_state->GetString(prefs::kWinAppRuntimePackageFamilyName);
    const std::string& min_version =
        local_state->GetString(prefs::kWinAppRuntimePackageMinVersion);
    bool package_up_to_date =
        family_name == base::WideToUTF8(kWinAppRuntimePackageFamilyName) &&
        min_version == kWinAppRuntimePackageMinVersionString;
    if (package_up_to_date) {
      RecordInstallState(WinAppRuntimeInstallStateUma::kRuntimeAlreadyPresent);
      return;
    }

    DeletePackageDependency(base::UTF8ToWide(dependency_id));
  }

  // Before starting the installation, first try to create the package
  // dependency, in case it has already been installed by another application on
  // the system.
  std::wstring new_dependency_id = TryCreateWinAppRuntimePackageDependency();
  if (!new_dependency_id.empty()) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kRuntimeAlreadyPresent);
    UpdatePrefs(new_dependency_id);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ActivateAppInstallManager),
      base::BindOnce(&StartInstallation));
}

}  // namespace

void SchedulePlatformRuntimeInstallationIfRequired() {
  if (!base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork) ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNOnnxRuntime)) {
    return;
  }

  if (base::win::GetVersion() < base::win::Version::WIN11_24H2) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kWindowsVersionTooOld);
    return;
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce(&EnsureInstallation));
}

}  // namespace webnn
