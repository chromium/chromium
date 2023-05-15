// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_STORAGE_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_STORAGE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/device_management/dm_message.h"

namespace wireless_android_enterprise_devicemanagement {
class OmahaSettingsClientProto;
}

namespace updater {

class CachedPolicyInfo;

// The token service interface defines how to serialize tokens.
class TokenServiceInterface {
 public:
  virtual ~TokenServiceInterface() = default;

  // ID of the device that the tokens target to.
  virtual std::string GetDeviceID() const = 0;

  // Checks if enrollment is mandatory.
  virtual bool IsEnrollmentMandatory() const = 0;

  // Writes |enrollment_token| to storage.
  virtual bool StoreEnrollmentToken(const std::string& enrollment_token) = 0;

  // Reads the enrollment token from sources as-needed to find one.
  // Returns an empty string if no enrollment token is found.
  virtual std::string GetEnrollmentToken() const = 0;

  // Writes |dm_token| into storage.
  virtual bool StoreDmToken(const std::string& dm_token) = 0;

  // Deletes the DM token from storage.
  virtual bool DeleteDmToken() = 0;

  // Returns the device management token from storage, or returns an empty
  // string if no device management token is found.
  virtual std::string GetDmToken() const = 0;
};

// The DMStorage is responsible for serialization of:
//   1) DM enrollment token.
//   2) DM token.
//   3) DM policies.
class DMStorage : public base::RefCountedThreadSafe<DMStorage> {
 public:
  explicit DMStorage(const base::FilePath& policy_cache_root);
  DMStorage(const base::FilePath& policy_cache_root,
            std::unique_ptr<TokenServiceInterface> token_service);
  DMStorage(const DMStorage&) = delete;
  DMStorage& operator=(const DMStorage&) = delete;

  // Forwards to token service to get device ID
  std::string GetDeviceID() const { return token_service_->GetDeviceID(); }

  // Forwards to token service to check if enrollment is mandatory.
  bool IsEnrollmentMandatory() const {
    return token_service_->IsEnrollmentMandatory();
  }

  // Forwards to token service to save enrollment token.
  bool StoreEnrollmentToken(const std::string& enrollment_token) {
    return token_service_->StoreEnrollmentToken(enrollment_token);
  }

  // Forwards to token service to get enrollment token.
  std::string GetEnrollmentToken() const {
    return token_service_->GetEnrollmentToken();
  }

  // Forwards to token service to save DM token.
  bool StoreDmToken(const std::string& dm_token) {
    return token_service_->StoreDmToken(dm_token);
  }

  // Forwards to token service to get DM token.
  std::string GetDmToken() const { return token_service_->GetDmToken(); }

  // Writes a special DM token to storage to mark current device as
  // deregistered.
  bool InvalidateDMToken();

  // Deletes the existing DM token for re-registration.
  bool DeleteDMToken();

  // Returns true if the DM token is valid, where valid is defined as non-blank
  // and not de-registered.
  bool IsValidDMToken() const;

  // Returns true if the device is de-registered.
  bool IsDeviceDeregistered() const;

  // Checks if the caller has permissions to persist the DM policies.
  bool CanPersistPolicies() const;

  // Persists DM policies.
  //
  // If the first policy in the map contains a valid public key, its serialized
  // data will be saved into a fixed file named "CachedPolicyInfo" in the cache
  // root. The file content will be used to construct an
  // updater::CachedPolicyInfo object to get public key, its version, and
  // signing timestamp. The values will be used in subsequent policy fetches.
  //
  // Each entry in |policy_map| will be stored within a sub-directory named
  // {Base64Encoded{policy_type}}, with a fixed file name of
  // "PolicyFetchResponse", where the file contents are serialized data of
  // the policy object.
  //
  // Please note that this function also purges all stale polices whose policy
  // type does not appear in keys of |policies|.
  //
  // Visualized directory structure example:
  //  <policy_cache_root_>
  //   |-- CachedPolicyInfo                      # Policy meta-data file.
  //   |-- Z29vZ2xlL21hY2hpbmUtbGV2ZWwtb21haGE=
  //   |       `--PolicyFetchResponse            # Policy response data.
  //   `-- Zm9vYmFy                              # b64("foobar").
  //           `--PolicyFetchResponse            # Policy response data.
  //
  //  ('Z29vZ2xlL21hY2hpbmUtbGV2ZWwtb21haGE=' is base64 encoding of
  //  "google/machine-level-omaha").
  //
  bool PersistPolicies(const DMPolicyMap& policy_map) const;

  // Creates a CachedPolicyInfo object and populates it with the public key
  // information loaded from file |policy_cache_root_|\CachedPolicyInfo.
  std::unique_ptr<CachedPolicyInfo> GetCachedPolicyInfo() const;

  // Returns the Omaha policy settings loaded from PolicyFetchResponse file in
  // |policy_cache_root_|\{Base64Encoded{kGoogleUpdatePolicyType}} directory.
  std::unique_ptr<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
  GetOmahaPolicySettings() const;

  // Returns the folder that caches the downloaded policies.
  base::FilePath policy_cache_folder() const { return policy_cache_root_; }

 private:
  friend class base::RefCountedThreadSafe<DMStorage>;
  ~DMStorage();

  const base::FilePath policy_cache_root_;
  const base::FilePath policy_info_file_;
  std::unique_ptr<TokenServiceInterface> token_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Returns the DMStorage under which the Device Management policies are
// persisted. For Windows, this is `%ProgramFiles(x86)%\{CompanyName}\Policies`.
// For macOS, this is `/Library/{CompanyName}/{KEYSTONE_NAME}/DeviceManagement`.
scoped_refptr<DMStorage> GetDefaultDMStorage();

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_STORAGE_H_
