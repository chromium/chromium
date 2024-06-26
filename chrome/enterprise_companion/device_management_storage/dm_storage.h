// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_DEVICE_MANAGEMENT_STORAGE_DM_STORAGE_H_
#define CHROME_ENTERPRISE_COMPANION_DEVICE_MANAGEMENT_STORAGE_DM_STORAGE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace device_management_storage {

// DM policy map: policy_type --> serialized policy data of PolicyFetchResponse.
using DMPolicyMap = base::flat_map<std::string, std::string>;

bool CreateGlobalAccessibleDirectory(const base::FilePath& path);
bool WriteContentToGlobalReadableFile(const base::FilePath& path,
                                      const std::string& content_to_write);

class CachedPolicyInfo {
 public:
  CachedPolicyInfo() = default;
  ~CachedPolicyInfo() = default;

  // Populate members with serialized data of DM PolicyFetchResponse.
  bool Populate(const std::string& raw_response);

  // Public key of the policy.
  std::string public_key() const { return key_; }

  // Version of the public key. -1 means the key is not versioned or unknown.
  int32_t key_version() const { return key_version_; }

  bool has_key_version() const { return key_version_ >= 0; }

  // Signing timestamp.
  int64_t timestamp() const { return timestamp_; }

 private:
  std::string key_;
  int32_t key_version_ = -1;
  int64_t timestamp_ = 0;
};

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

  // Deletes |enrollment_token| from storage.
  virtual bool DeleteEnrollmentToken() = 0;

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
  static constexpr size_t kMaxDmTokenLength = 4096;

  // Forwards to token service to get device ID
  virtual std::string GetDeviceID() const = 0;

  // Forwards to token service to check if enrollment is mandatory.
  virtual bool IsEnrollmentMandatory() const = 0;

  // Forwards to token service to save enrollment token.
  virtual bool StoreEnrollmentToken(const std::string& enrollment_token) = 0;

  // Forwards to token service to delete enrollment token.
  virtual bool DeleteEnrollmentToken() = 0;

  // Forwards to token service to get enrollment token.
  virtual std::string GetEnrollmentToken() const = 0;

  // Forwards to token service to save DM token.
  virtual bool StoreDmToken(const std::string& dm_token) = 0;

  // Forwards to token service to get DM token.
  virtual std::string GetDmToken() const = 0;

  // Writes a special DM token to storage to mark current device as
  // deregistered.
  virtual bool InvalidateDMToken() = 0;

  // Deletes the existing DM token for re-registration.
  virtual bool DeleteDMToken() = 0;

  // Returns true if the DM token is valid, where valid is defined as non-blank
  // and not de-registered.
  virtual bool IsValidDMToken() const = 0;

  // Returns true if the device is de-registered.
  virtual bool IsDeviceDeregistered() const = 0;

  // Checks if the caller has permissions to persist the DM policies.
  virtual bool CanPersistPolicies() const = 0;

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
  virtual bool PersistPolicies(const DMPolicyMap& policy_map) const = 0;

  // Removes all the cached policies, including the cached policy info.
  virtual bool RemoveAllPolicies() const = 0;

  // Creates a CachedPolicyInfo object and populates it with the public key
  // information loaded from file |policy_cache_root_|\CachedPolicyInfo.
  virtual std::unique_ptr<CachedPolicyInfo> GetCachedPolicyInfo() const = 0;

  // Returns the policy data loaded from the PolicyFetchResponse file in the
  // |policy_cache_root_|\{Base64Encoded{|policy_type|}} directory.
  virtual std::optional<enterprise_management::PolicyData> ReadPolicyData(
      const std::string& policy_type) = 0;

  // Returns the folder that caches the downloaded policies.
  virtual base::FilePath policy_cache_folder() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DMStorage>;
  virtual ~DMStorage() = default;
};

#if BUILDFLAG(IS_WIN)
scoped_refptr<DMStorage> CreateDMStorage(
    const base::FilePath& policy_cache_root);
#else
scoped_refptr<DMStorage> CreateDMStorage(
    const base::FilePath& policy_cache_root,
    const base::FilePath& enrollment_token_path = {},
    const base::FilePath& dm_token_path = {});
#endif
scoped_refptr<DMStorage> CreateDMStorage(
    const base::FilePath& policy_cache_root,
    std::unique_ptr<TokenServiceInterface> token_service);

// Returns the DMStorage under which the Device Management policies are
// persisted. For Windows, this is `%ProgramFiles(x86)%\{CompanyName}\Policies`.
// For macOS, this is `/Library/{CompanyName}/{KEYSTONE_NAME}/DeviceManagement`.
scoped_refptr<DMStorage> GetDefaultDMStorage();

}  // namespace device_management_storage

#endif  // CHROME_ENTERPRISE_COMPANION_DEVICE_MANAGEMENT_STORAGE_DM_STORAGE_H_
