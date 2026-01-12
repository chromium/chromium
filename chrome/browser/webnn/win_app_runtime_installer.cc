// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webnn/win_app_runtime_installer.h"

#include <windows.h>

#include <Windows.ApplicationModel.store.preview.installcontrol.h>
#include <wrl.h>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/webnn/webnn_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/public/mojom/features.mojom-features.h"

#if BUILDFLAG(WEBNN_INSTALL_RUNTIME_IN_CHROME_INSTALLER)
#include "chrome/installer/util/helper.h"  // nogncheck
#endif  // BUILDFLAG(WEBNN_INSTALL_RUNTIME_IN_CHROME_INSTALLER)

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
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kWinAppRuntimePackageDependencyId,
                         base::WideToUTF8(dependency_id));
  local_state->SetString(prefs::kWinAppRuntimePackageFamilyName,
                         base::WideToUTF8(kWinAppRuntimePackageFamilyName));
  local_state->SetString(prefs::kWinAppRuntimePackageMinVersion,
                         kWinAppRuntimePackageMinVersionString);
}

// Called when `AppInstallItem` reaches the completed state.
void OnInstallationSucceeded() {
  std::wstring dependency_id = TryCreateWinAppRuntimePackageDependency();
  if (dependency_id.empty()) {
    return;
  }
  UpdatePrefs(dependency_id);
}

// Implements the AppInstallStatusChangedHandler.
class AppInstallStatusChangedHandlerImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          AppInstallStatusChangedHandler> {
 public:
  AppInstallStatusChangedHandlerImpl(
      std::unique_ptr<EventRegistrationToken> token,
      Microsoft::WRL::ComPtr<abi_install::IAppInstallManager>
          app_install_manager)
      : token_(std::move(token)),
        app_install_manager_(std::move(app_install_manager)) {}

  IFACEMETHODIMP Invoke(abi_install::IAppInstallItem* sender,
                        IInspectable* args) override {
    Microsoft::WRL::ComPtr<abi_install::IAppInstallStatus> status;
    HRESULT hr = sender->GetCurrentStatus(&status);
    CHECK_EQ(hr, S_OK);

    abi_install::AppInstallState state;
    hr = status->get_InstallState(&state);
    CHECK_EQ(hr, S_OK);

    switch (state) {
      case abi_install::AppInstallState::AppInstallState_Completed: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kCompleted);
        OnCompleted(sender, /*success=*/true);
        break;
      }
      case abi_install::AppInstallState::AppInstallState_Error: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kError);
        OnCompleted(sender, /*success=*/false);
        break;
      }
      case abi_install::AppInstallState::AppInstallState_Canceled: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kCanceled);
        OnCompleted(sender, /*success=*/false);
        break;
      }
      case abi_install::AppInstallState::AppInstallState_Paused: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kPaused);
        break;
      }
      case abi_install::AppInstallState::AppInstallState_PausedLowBattery: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kPausedLowBattery);
        break;
      }
      case abi_install::AppInstallState::
          AppInstallState_PausedWiFiRecommended: {
        RecordInstallState(
            WinAppRuntimeInstallStateUma::kPausedWiFiRecommended);
        break;
      }
      case abi_install::AppInstallState::AppInstallState_PausedWiFiRequired: {
        RecordInstallState(WinAppRuntimeInstallStateUma::kPausedWiFiRequired);
        break;
      }
      default:
        break;
    }
    return S_OK;
  }

 private:
  // Posts a task to continue processing on the UI thread. Does nothing if
  // called more than once.
  void OnCompleted(abi_install::IAppInstallItem* sender, bool success) {
    base::AutoLock auto_lock(lock_);
    if (app_install_manager_) {
      sender->remove_StatusChanged(*token_);
      app_install_manager_ = nullptr;

      if (success) {
        content::GetUIThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(&OnInstallationSucceeded));
      }
    }
  }

  // Used to unregister the status changed handler.
  const std::unique_ptr<EventRegistrationToken> token_;
  // Used to ensure `OnInstallationSucceeded()` is run only once since
  // `Invoke()` can be called on different background threads.
  base::Lock lock_;
  // `app_install_manager_` is kept alive here to ensure the registered status
  // changed callbacks will be invoked.
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager_
      GUARDED_BY(lock_);
};

// Called after `StartProductInstallAsync()` completes. Adds a callback for
// `AppInstallItem` to track the installation status.
void OnInstallationStarted(
    Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager,
    Microsoft::WRL::ComPtr<AppInstallItems> items) {
  uint32_t count = 0;
  HRESULT hr = items->get_Size(&count);
  CHECK_EQ(hr, S_OK);

  for (uint32_t i = 0; i < count; ++i) {
    Microsoft::WRL::ComPtr<abi_install::IAppInstallItem> item;
    hr = items->GetAt(i, &item);
    CHECK_EQ(hr, S_OK);

    // Checks if the install item is the package that we want.
    base::win::ScopedHString product_id(nullptr);
    hr = item->get_ProductId(
        base::win::ScopedHString::Receiver(product_id).get());
    CHECK_EQ(hr, S_OK);
    if (product_id.Get() != kWinAppRuntimeProductId) {
      continue;
    }

    // `token` receives the value assigned by `add_StatusChanged()` below.
    auto token = std::make_unique<EventRegistrationToken>();
    auto* token_ptr = token.get();

    // Register the status change event handler for `item`.
    item->add_StatusChanged(
        Microsoft::WRL::Make<AppInstallStatusChangedHandlerImpl>(
            std::move(token), std::move(app_install_manager))
            .Get(),
        token_ptr);

    return;
  }

  RecordInstallState(WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
}

// Starts the installation using the IAppInstallManager API.
void StartInstallation() {
  HRESULT hr = S_OK;
  // `app_install_manager` must remain valid throughout the entire
  // asynchronous installation process, otherwise WinRT may release related
  // resources prematurely, causing the callbacks not to be triggered.
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager;
  {
    // `RoActivateInstance()` below loads Microsoft Store Install Service dlls.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    hr = base::win::RoActivateInstance(
        base::win::HStringReference(
            RuntimeClass_Windows_ApplicationModel_Store_Preview_InstallControl_AppInstallManager)
            .Get(),
        &app_install_manager);
  }
  if (FAILED(hr)) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kActivationFailure);
    return;
  }

  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager3>
      app_install_manager_3;
  hr = app_install_manager.As(&app_install_manager_3);
  CHECK_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<AppInstallAsyncOp> async_op;
  hr = app_install_manager_3->StartProductInstallAsync(
      base::win::HStringReference(kWinAppRuntimeProductId.c_str()).Get(),
      /*catalogId=*/nullptr, /*flightId=*/nullptr, /*clientId=*/nullptr,
      /*repair=*/false, /*forceUseOfNonRemovableStorage=*/false,
      /*correlationVector=*/nullptr, /*targetVolume=*/nullptr, &async_op);
  if (FAILED(hr)) {
    RecordInstallState(
        WinAppRuntimeInstallStateUma::kInstallationFailedToStart);
    return;
  }

  // Post a handler to wait for the completion of `StartProductInstallAsync()`.
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
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
  // dependency to check if it has already been installed.
  std::wstring new_dependency_id = TryCreateWinAppRuntimePackageDependency();
  if (!new_dependency_id.empty()) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kRuntimeAlreadyPresent);
    UpdatePrefs(new_dependency_id);
    return;
  }

  // Post the installation to a Multi-Threaded Apartment (MTA) background
  // thread to prevent a potential hung issue on a Single-Threaded Apartment
  // (STA) thread, see crbug.com/464001953.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(&StartInstallation));
}

}  // namespace

void SchedulePlatformRuntimeInstallationIfRequired() {
  if (!base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork) ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNOnnxRuntime)) {
    return;
  }

  if (base::win::GetVersion() < kWinAppRuntimeSupportedMinVersion) {
    RecordInstallState(WinAppRuntimeInstallStateUma::kWindowsVersionTooOld);
    return;
  }

#if BUILDFLAG(WEBNN_INSTALL_RUNTIME_IN_CHROME_INSTALLER)
  // Check if the browser process was installed by the Chrome installer. If so,
  // skip the installation check here as the installer is responsible for
  // handling this. The installation in the browser process is only for
  // developer builds.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&installer::IsCurrentProcessInstalled),
      base::BindOnce([](bool is_browser_process_installed) {
        if (is_browser_process_installed) {
          return;
        }
        content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
            ->PostTask(FROM_HERE, base::BindOnce(&EnsureInstallation));
      }));
#else
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce(&EnsureInstallation));
#endif  // BUILDFLAG(WEBNN_INSTALL_RUNTIME_IN_CHROME_INSTALLER)
}

}  // namespace webnn
