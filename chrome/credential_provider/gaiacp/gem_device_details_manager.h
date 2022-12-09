// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_

#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to handle requests to store device details in GEM database.
class GemDeviceDetailsManager {
 public:
  // Default timeout when trying to make requests to the GEM cloud service
  // to upload device details from GCPW.
  static const base::TimeDelta kDefaultUploadDeviceDetailsRequestTimeout;

  static GemDeviceDetailsManager* Get();

  // Provides the GCPW extension with a TaskCreator which can be used to create
  // a task for uploading device details.
  static extension::TaskCreator UploadDeviceDetailsTaskCreator();

  // Upload device details to GEM database using access token.
  HRESULT UploadDeviceDetails(const std::string& access_token,
                              const std::wstring& sid,
                              const std::wstring& username,
                              const std::wstring& domain);

  // Upload device details to GEM database using dmToken.
  HRESULT UploadDeviceDetails(const extension::UserDeviceContext& context);

  // Set the upload device details http response status for the
  // purpose of unit testing.
  void SetUploadStatusForTesting(HRESULT hr) { upload_status_ = hr; }

  // Calculates the full url of various GEM service requests.
  GURL GetGemServiceUploadDeviceDetailsUrl();

  // Return true if upload device details feature is enabled in ESA.
  bool UploadDeviceDetailsFromEsaFeatureEnabled() const;

  // For testing manually control if the upload device details feature is
  // enabled in ESA.
  void SetUploadDeviceDetailsFromEsaFeatureEnabledForTesting(bool value);

 protected:
  // Returns the storage used for the instance pointer.
  static GemDeviceDetailsManager** GetInstanceStorage();

  explicit GemDeviceDetailsManager(
      base::TimeDelta upload_device_details_request_timeout);
  virtual ~GemDeviceDetailsManager();

  // Sets the timeout of http request to the GEM Service for the
  // purposes of unit testing.
  void SetRequestTimeoutForTesting(base::TimeDelta request_timeout) {
    upload_device_details_request_timeout_ = request_timeout;
  }

  // Gets the request dictionary used to invoke the GEM service for
  // the purpose of testing.
  const base::Value::Dict& GetRequestDictForTesting() { return *request_dict_; }

  // Get the upload device details http response status for the
  // purpose of unit testing.
  HRESULT GetUploadStatusForTesting() { return upload_status_; }

 private:
  base::TimeDelta upload_device_details_request_timeout_;
  HRESULT upload_status_;
  std::unique_ptr<base::Value::Dict> request_dict_;
  HRESULT UploadDeviceDetailsInternal(const std::string access_token,
                                      const std::wstring obfuscated_user_id,
                                      const std::wstring dm_token,
                                      const std::wstring sid,
                                      const std::wstring device_resource_id,
                                      const std::wstring username,
                                      const std::wstring domain);
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_
