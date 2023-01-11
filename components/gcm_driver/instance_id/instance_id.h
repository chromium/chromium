// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
//
// Create instances of this class by calling |InstanceIDDriver::GetInstanceID|.
class InstanceID {
 public:
  // Used in UMA. Can add enum values, but never renumber or delete and reuse.
  enum Result : uint8_t {
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

    // Used for UMA. Keep kMaxValue up to date and sync with histograms.xml.
    kMaxValue = UNKNOWN_ERROR
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
  using GetIDCallback = base::OnceCallback<void(const std::string& id)>;
  using GetCreationTimeCallback =
      base::OnceCallback<void(const base::Time& creation_time)>;
  using GetTokenCallback =
      base::OnceCallback<void(const std::string& token, Result result)>;
  using ValidateTokenCallback = base::OnceCallback<void(bool is_valid)>;
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

  InstanceID(const InstanceID&) = delete;
  InstanceID& operator=(const InstanceID&) = delete;

  virtual ~InstanceID();

  // Returns the Instance ID.
  virtual void GetID(GetIDCallback callback) = 0;

  // Returns the time when the Instance ID has been generated.
  virtual void GetCreationTime(GetCreationTimeCallback callback) = 0;

  // Retrieves a token that allows the authorized entity to access the service
  // defined as "scope". This may cause network requests but the result is
  // cached on disk for up to a week. Token validity will be checked
  // automatically. Thus you should not store tokens for long periods yourself,
  // instead call this function each time it's needed.
  //
  // To receive messages, register an |AppIdHandler| on |gcm_driver()|.
  //
  // |authorized_entity|: identifies the entity that is authorized to access
  //                      resources associated with this Instance ID. It can be
  //                      another Instance ID or a numeric project ID.
  // |scope|: identifies authorized actions that the authorized entity can take.
  //          E.g. for sending GCM messages, "GCM" scope should be used.
  // |time_to_live|: TTL of retrieved token, unlimited if zero value passed.
  // |flags|: Flags used to create this token.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void GetToken(const std::string& authorized_entity,
                        const std::string& scope,
                        base::TimeDelta time_to_live,
                        std::set<Flags> flags,
                        GetTokenCallback callback) = 0;

  // Checks that the provided |token| matches the stored token for (|app_id()|,
  // |authorized_entity|, |scope|). If you follow the guidance for |GetToken|,
  // and call that function each time you need the token, then you will not
  // need to use this function.
  virtual void ValidateToken(const std::string& authorized_entity,
                             const std::string& scope,
                             const std::string& token,
                             ValidateTokenCallback callback) = 0;

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

  gcm::GCMDriver* gcm_driver() { return gcm_driver_; }

 protected:
  InstanceID(const std::string& app_id, gcm::GCMDriver* gcm_driver);

  // Platform-specific implementations.
  virtual void DeleteTokenImpl(const std::string& authorized_entity,
                               const std::string& scope,
                               DeleteTokenCallback callback) = 0;
  virtual void DeleteIDImpl(DeleteIDCallback callback) = 0;

  void NotifyTokenRefresh(bool update_id);

 private:
  void DidDelete(const std::string& authorized_entity,
                 base::OnceCallback<void(Result result)> callback,
                 Result result);

  // Owned by GCMProfileServiceFactory, which is a dependency of
  // InstanceIDProfileServiceFactory, which owns this.
  raw_ptr<gcm::GCMDriver, DanglingUntriaged> gcm_driver_;

  std::string app_id_;

  base::WeakPtrFactory<InstanceID> weak_ptr_factory_{this};
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_H_
