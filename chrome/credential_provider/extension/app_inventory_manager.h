// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_APP_INVENTORY_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_APP_INVENTORY_MANAGER_H_

#include "base/time/time.h"
#include "base/values.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to upload app data to GCPW backends.
class AppInventoryManager {
 public:
  // Get the app inventory manager instance.
  static AppInventoryManager* Get();

  // Provides the GCPW extension with a TaskCreator which can be used to create
  // a task for uploading app data.
  static extension::TaskCreator UploadAppInventoryTaskCreator();

  // Return true if app inventory feature is enabled.
  bool UploadAppInventoryFromEsaFeatureEnabled() const;

  // Upload app data to GEM backend for device with |sid| and |resource_id|
  // using |dm_token|for authentication and authorization.
  virtual HRESULT UploadAppInventory(
      const extension::UserDeviceContext& context);

  // Get the URL of GEM service for HTTP request for uploading app data.
  GURL GetGemServiceUploadAppInventoryUrl();

  // For testing manually control if the app inventory feature is enabled.
  void SetUploadAppInventoryFromEsaFeatureEnabledForTesting(bool value);

  // Get this list of installed apps on the device. This function reads
  // registries values to get the list of installed win32 apps.
  base::Value GetInstalledWin32Apps();

  // Set fakes for cloud policies unit tests.
  void SetFakesForTesting(FakesForTesting* fakes);

 protected:
  // Returns the storage used for the instance pointer.
  static AppInventoryManager** GetInstanceStorage();

  explicit AppInventoryManager(
      base::TimeDelta upload_app_inventory_request_timeout_);
  virtual ~AppInventoryManager();

  HRESULT fetch_status_;

 private:
  base::TimeDelta upload_app_inventory_request_timeout_;
  std::unique_ptr<base::Value::Dict> request_dict_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_APP_INVENTORY_MANAGER_H_
