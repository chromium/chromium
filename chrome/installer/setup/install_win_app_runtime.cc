// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_win_app_runtime.h"

#include <windows.h>

#include <Windows.ApplicationModel.store.preview.installcontrol.h>
#include <appmodel.h>
#include <wrl/client.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/windows_version.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"

namespace installer {

namespace {

namespace abi_install =
    ::ABI::Windows::ApplicationModel::Store::Preview::InstallControl;

using AppInstallAsyncOp =
    __FIAsyncOperation_1___FIVectorView_1_Windows__CApplicationModel__CStore__CPreview__CInstallControl__CAppInstallItem;

// Triggers the installation for Windows App Runtime. Installs the package for
// all users if `system_install` is true.
bool TriggerInstallation(bool system_install) {
  Microsoft::WRL::ComPtr<abi_install::IAppInstallManager> app_install_manager;
  HRESULT hr = base::win::RoActivateInstance(
      base::win::HStringReference(
          RuntimeClass_Windows_ApplicationModel_Store_Preview_InstallControl_AppInstallManager)
          .Get(),
      &app_install_manager);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to activate IAppInstallManager instance.";
    return false;
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
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to activate IAppInstallOptions instance.";
    return false;
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
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start Windows App Runtime installation.";
    return false;
  }

  VLOG(1) << "Successfully started Windows App Runtime installation.";
  return true;
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
  if (!TriggerInstallation(system_install)) {
    return;
  }

  // Creates a package dependency for the version folder without verifying the
  // dependency resolution since the installation is asynchronous and most
  // likely has not completed.
  CreatePackageDependencyOptions options =
      CreatePackageDependencyOptions_DoNotVerifyDependencyResolution;
  if (system_install) {
    options |= CreatePackageDependencyOptions_ScopeIsSystem;
  }
  std::wstring dependency_id = webnn::TryCreatePackageDependencyForFilePath(
      webnn::kWinAppRuntimePackageFamilyName,
      webnn::kWinAppRuntimePackageMinVersion, version_dir, options);
  LOG_IF(ERROR, dependency_id.empty())
      << "Failed to create package dependency for Windows App Runtime.";
}

}  // namespace installer
