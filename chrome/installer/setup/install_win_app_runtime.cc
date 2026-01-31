// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_win_app_runtime.h"

#include <windows.h>

#include <Windows.ApplicationModel.store.preview.installcontrol.h>
#include <appmodel.h>
#include <wrl.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"

namespace installer {

namespace {

namespace abi_install =
    ::ABI::Windows::ApplicationModel::Store::Preview::InstallControl;

using AppInstallAsyncOp =
    __FIAsyncOperation_1___FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;
using AppStartInstallCompletedHandler =
    __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;
using AppInstallItems =
    __FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;

enum class Operation {
  kActivateManager,
  kActivateOptions,
  kStartInstall,
  kWaitForStartInstall,
};

std::string_view ToString(Operation operation) {
  switch (operation) {
    case Operation::kActivateManager:
      return "ActivateManager";
    case Operation::kActivateOptions:
      return "ActivateOptions";
    case Operation::kStartInstall:
      return "StartInstall";
    case Operation::kWaitForStartInstall:
      return "WaitForStartInstall";
  }
}

void RecordResult(Operation operation, HRESULT result) {
  base::UmaHistogramSparse(base::StrCat({"Setup.Install.WinAppRuntime.",
                                         ToString(operation), ".Result"}),
                           result);
}

// Gets the result of the async operation.
HRESULT GetResult(AppInstallAsyncOp* async_op, AsyncStatus status) {
  if (status == AsyncStatus::Completed) {
    Microsoft::WRL::ComPtr<AppInstallItems> items;
    HRESULT hr = async_op->GetResults(&items);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get results from the async operation. Error: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    uint32_t count = 0;
    hr = items->get_Size(&count);
    CHECK_EQ(hr, S_OK);

    for (uint32_t i = 0; i < count; ++i) {
      Microsoft::WRL::ComPtr<abi_install::IAppInstallItem> item;
      hr = items->GetAt(i, &item);
      CHECK_EQ(hr, S_OK);

      // Check if the install item is the package that we want.
      base::win::ScopedHString product_id(nullptr);
      hr = item->get_ProductId(
          base::win::ScopedHString::Receiver(product_id).get());
      CHECK_EQ(hr, S_OK);
      if (product_id.Get() == webnn::kWinAppRuntimeProductId) {
        VLOG(1) << "Successfully started Windows App Runtime installation.";
        return S_OK;
      }
    }
    LOG(ERROR) << "The async operation completed but no matching "
                  "install item was found.";
    return E_FAIL;
  }

  Microsoft::WRL::ComPtr<::ABI::Windows::Foundation::IAsyncInfo> async_info;
  HRESULT hr = async_op->QueryInterface(IID_PPV_ARGS(&async_info));
  CHECK_EQ(hr, S_OK);

  HRESULT error_code = S_OK;
  hr = async_info->get_ErrorCode(&error_code);
  CHECK_EQ(hr, S_OK);

  LOG(ERROR) << "Failed to complete the async operation. Error: "
             << logging::SystemErrorCodeToString(error_code);
  return error_code;
}

// Implements the AppStartInstallCompletedHandler. The `IAgileObject` interface
// allows this handler to be invoked on any COM apartment, ensuring it's not
// called on the main thread which runs in a Single-Threaded Apartment (STA) and
// has been blocked.
class AppInstallCompletedHandlerImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          AppStartInstallCompletedHandler,
          IAgileObject> {
 public:
  AppInstallCompletedHandlerImpl(bool system_install,
                                 const base::FilePath& version_dir,
                                 base::win::ScopedHandle completion_handle)
      : system_install_(system_install),
        version_dir_(version_dir),
        completion_event_(std::move(completion_handle)) {}

  IFACEMETHODIMP Invoke(AppInstallAsyncOp* async_op,
                        AsyncStatus status) override {
    if (status == AsyncStatus::Started) {
      return S_OK;
    }

    HRESULT result = GetResult(async_op, status);
    RecordResult(Operation::kWaitForStartInstall, result);

    if (result == S_OK) {
      // Create a package dependency for the version folder without verifying
      // dependency resolution, as the installation is asynchronous and may not
      // complete immediately.
      CreatePackageDependencyOptions options =
          CreatePackageDependencyOptions_DoNotVerifyDependencyResolution;
      if (system_install_) {
        options |= CreatePackageDependencyOptions_ScopeIsSystem;
      }
      std::wstring dependency_id = webnn::TryCreatePackageDependencyForFilePath(
          webnn::kWinAppRuntimePackageFamilyName,
          webnn::kWinAppRuntimePackageMinVersion, version_dir_, options);
      LOG_IF(ERROR, dependency_id.empty())
          << "Failed to create package dependency for Windows App Runtime.";
    }

    // Notify the main thread that the operation has completed, so it can
    // stop waiting.
    completion_event_.Signal();

    return S_OK;
  }

 private:
  const bool system_install_;
  const base::FilePath version_dir_;
  base::WaitableEvent completion_event_;
};

// Triggers the installation for Windows App Runtime. Installs the package for
// all users if `system_install` is true.
void TriggerInstallation(bool system_install,
                         const base::FilePath& version_dir) {
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager;
  HRESULT hr = base::win::RoActivateInstance(
      base::win::HStringReference(
          RuntimeClass_Windows_ApplicationModel_Store_Preview_InstallControl_AppInstallManager)
          .Get(),
      &app_install_manager);
  RecordResult(Operation::kActivateManager, hr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to activate IAppInstallManager instance. Error: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager6>
      app_install_manager_6;
  hr = app_install_manager.As(&app_install_manager_6);
  CHECK_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<abi_install::IAppInstallOptions> install_options;
  hr = base::win::RoActivateInstance(
      base::win::HStringReference(
          RuntimeClass_Windows_ApplicationModel_Store_Preview_InstallControl_AppInstallOptions)
          .Get(),
      &install_options);
  RecordResult(Operation::kActivateOptions, hr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to activate IAppInstallOptions instance. Error: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  if (system_install) {
    Microsoft::WRL::ComPtr<abi_install::IAppInstallOptions2> install_options_2;
    hr = install_options.As(&install_options_2);
    CHECK_EQ(hr, S_OK);
    hr = install_options_2->put_InstallForAllUsers(true);
    CHECK_EQ(hr, S_OK);
  }

  Microsoft::WRL::ComPtr<AppInstallAsyncOp> async_op;
  hr = app_install_manager_6->StartProductInstallWithOptionsAsync(
      base::win::HStringReference(webnn::kWinAppRuntimeProductId.c_str()).Get(),
      /*flightId=*/nullptr, /*clientId=*/nullptr,
      /*correlationVector=*/nullptr, install_options.Get(), &async_op);
  RecordResult(Operation::kStartInstall, hr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start Windows App Runtime installation. Error: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  base::WaitableEvent completion_event;
  // Duplicate the handle so the completed handler below can still use it in
  // a background thread after `completion_event` goes out of scope.
  HANDLE completion_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), completion_event.handle(),
                         ::GetCurrentProcess(), &completion_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    RecordResult(Operation::kWaitForStartInstall,
                 HRESULT_FROM_WIN32(::GetLastError()));
    PLOG(ERROR) << "Failed to duplicate the handle.";
    return;
  }

  // Post a handler to wait for the installation to be initiated. Note that
  // this does not wait for the installation to complete. The browser will
  // verify the installation status during launch by attempting to create a
  // dependency on the package. For more details, see `EnsureInstallation()`
  // in //chrome/browser/webnn/win_app_runtime_installer.cc.
  hr = async_op->put_Completed(
      Microsoft::WRL::Make<AppInstallCompletedHandlerImpl>(
          system_install, version_dir,
          base::win::ScopedHandle(std::exchange(completion_handle, nullptr)))
          .Get());
  if (FAILED(hr)) {
    RecordResult(Operation::kWaitForStartInstall, hr);
    LOG(ERROR) << "Failed to post the completed handler. Error: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  base::ScopedUmaHistogramTimer histogram_timer(
      "Setup.Install.WinAppRuntime.WaitForStartInstall.TimingMs");

  // Block the main thread to wait for the completed handler to execute.
  // This ensures the package is added to the installation queue.
  if (!completion_event.TimedWait(base::Seconds(10))) {
    RecordResult(Operation::kWaitForStartInstall,
                 HRESULT_FROM_WIN32(WAIT_TIMEOUT));
    LOG(ERROR) << "Timed out when waiting for the completed handler.";
  }
}

}  // namespace

void MaybeTriggerWinAppRuntimeInstallation(bool system_install,
                                           const base::FilePath& version_dir) {
  if (base::win::GetVersion() < webnn::kWinAppRuntimeSupportedMinVersion) {
    return;
  }

  // There is no effective way to check if the package has been installed for
  // all users when it's system install, so here we directly trigger the
  // installation for simplicity. The installation is expected to complete right
  // away if the package is already present.
  TriggerInstallation(system_install, version_dir);
}

}  // namespace installer
