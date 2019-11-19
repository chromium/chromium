// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {

extern const char kGCMScope[];

// Encapsulates Instance ID functionalities that need to be implemented for
// different platforms. One instance is created per application. Life of
// Instance ID is managed by the InstanceIDDriver.
class InstanceID {
 public:
  // Used in UMA. Can add enum values, but never renumber or delete and reuse.
  enum Result {
    // Successful operation.
    SUCCESS = 0,
    // Invalid parameter.
    INVALID_PARAMETER = 1,
    // Instance ID is disabled.
    DISABLED = 2,
    // Previous asynchronous operation is still pending to finish.
    ASYNC_OPERATION_PENDING = 3,
    // Network socket error.
    NETWORK_ERROR = 4,
    // Problem at the server.
    SERVER_ERROR = 5,
    // 6 is omitted, in case we ever merge this enum with GCMClient::Result.
    // Other errors.
    UNKNOWN_ERROR = 7,

    // Used for UMA. Keep LAST_RESULT up to date and sync with histograms.xml.
    LAST_RESULT = UNKNOWN_ERROR
  };

  // Flags to be used to create a token. These might be platform specific.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.gcm_driver
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: InstanceIDFlags
  enum class Flags {
    // Whether delivery of received messages should be deferred until there is a
    // visible activity. Only applicable for Android.
    kIsLazy = 1 << 0,
    // Whether delivery of received messages should bypass the background task
    // scheduler. Only applicable for high priority messages on Android.
    kBypassScheduler = 1 << 1,
  };

  // Asynchronous callbacks. Must not synchronously delete |this| (using
  // InstanceIDDriver::RemoveInstanceID).
  using TokenRefreshCallback =
      base::Callback<void(const std::string& app_id, bool update_id)>;
  using GetIDCallback = base::Callback<void(const std::string& id)>;
  using GetCreationTimeCallback =
      base::Callback<void(const base::Time& creation_time)>;
  using GetTokenCallback =
      base::OnceCallback<void(const std::string& token, Result result)>;
  using ValidateTokenCallback = base::Callback<void(bool is_valid)>;
  using GetEncryptionInfoCallback =
      base::OnceCallback<void(std::string p256dh, std::string auth_secret)>;
  using DeleteTokenCallback = base::OnceCallback<void(Result result)>;
  using DeleteIDCallback = base::OnceCallback<void(Result result)>;

  static const int kInstanceIDByteLength = 8;

  // Creator. Should only be used by InstanceIDDriver::GetInstanceID.
  // |app_id|: identifies the application that uses the Instance ID.
  // |handler|: provides the GCM functionality needed to support Instance ID.
  //            Must outlive this class. On Android, this can be null instead.
  static std::unique_ptr<InstanceID> CreateInternal(const std::string& app_id,
                                                    gcm::GCMDriver* gcm_driver);

  virtual ~InstanceID();

  // Sets the callback that will be invoked when the token refresh event needs
  // to be triggered.
  void SetTokenRefreshCallback(const TokenRefreshCallback& callback);

  // Returns the Instance ID.
  virtual void GetID(const GetIDCallback& callback) = 0;

  // Returns the time when the InstanceID has been generated.
  virtual void GetCreationTime(const GetCreationTimeCallback& callback) = 0;

  // Retrieves a token that allows the authorized entity to access the service
  // defined as "scope".
  // |authorized_entity|: identifies the entity that is authorized to access
  //                      resources associated with this Instance ID. It can be
  //                      another Instance ID or a project ID.
  // |scope|: identifies authorized actions that the authorized entity can take.
  //          E.g. for sending GCM messages, "GCM" scope should be used.
  // |options|: allows including a small number of string key/value pairs that
  //            will be associated with the token and may be used in processing
  //            the request.
  // |flags|: Flags used to create this token.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void GetToken(const std::string& authorized_entity,
                        const std::string& scope,
                        const std::map<std::string, std::string>& options,
                        std::set<Flags> flags,
                        GetTokenCallback callback) = 0;

  // Checks that the provided |token| matches the stored token for (|app_id()|,
  // |authorized_entity|, |scope|).
  virtual void ValidateToken(const std::string& authorized_entity,
                             const std::string& scope,
                             const std::string& token,
                             const ValidateTokenCallback& callback) = 0;

  // Get the public encryption key and authentication secret associated with a
  // GCM-scoped token. If encryption info is not yet associated, it will be
  // created.
  // |authorized_entity|: the authorized entity passed when obtaining the token.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void GetEncryptionInfo(const std::string& authorized_entity,
                                 GetEncryptionInfoCallback callback);

  // Revokes a granted token.
  // |authorized_entity|: the authorized entity passed when obtaining the token.
  // |scope|: the scope that was passed when obtaining the token.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void DeleteToken(const std::string& authorized_entity,
                           const std::string& scope,
                           DeleteTokenCallback callback);

  // Resets the app instance identifier and revokes all tokens associated with
  // it.
  // |callback|: to be called once the asynchronous operation is done.
  void DeleteID(DeleteIDCallback callback);

  std::string app_id() const { return app_id_; }

 protected:
  InstanceID(const std::string& app_id, gcm::GCMDriver* gcm_driver);

  // Platform-specific implementations.
  virtual void DeleteTokenImpl(const std::string& authorized_entity,
                               const std::string& scope,
                               DeleteTokenCallback callback) = 0;
  virtual void DeleteIDImpl(DeleteIDCallback callback) = 0;

  void NotifyTokenRefresh(bool update_id);

  gcm::GCMDriver* gcm_driver() { return gcm_driver_; }

 private:
  void DidDelete(const std::string& authorized_entity,
                 base::OnceCallback<void(Result result)> callback,
                 Result result);

  // Owned by GCMProfileServiceFactory, which is a dependency of
  // InstanceIDProfileServiceFactory, which owns this.
  gcm::GCMDriver* gcm_driver_;

  std::string app_id_;
  TokenRefreshCallback token_refresh_callback_;

  base::WeakPtrFactory<InstanceID> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InstanceID);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_
