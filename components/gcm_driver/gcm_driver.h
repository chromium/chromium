// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_client.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace gcm {

class GCMAppHandler;
class GCMConnectionObserver;
enum class GCMDecryptionResult;
enum class GCMEncryptionResult;
struct AccountMapping;

// Provides the InstanceID support via GCMDriver.
class InstanceIDHandler {
 public:
  using GetTokenCallback = base::OnceCallback<void(const std::string& token,
                                                   GCMClient::Result result)>;
  using ValidateTokenCallback = base::OnceCallback<void(bool is_valid)>;
  using DeleteTokenCallback =
      base::OnceCallback<void(GCMClient::Result result)>;
  using GetInstanceIDDataCallback =
      base::OnceCallback<void(const std::string& instance_id,
                              const std::string& extra_data)>;

  InstanceIDHandler();

  InstanceIDHandler(const InstanceIDHandler&) = delete;
  InstanceIDHandler& operator=(const InstanceIDHandler&) = delete;

  virtual ~InstanceIDHandler();

  // Token service.
  virtual void GetToken(const std::string& app_id,
                        const std::string& authorized_entity,
                        const std::string& scope,
                        base::TimeDelta time_to_live,
                        GetTokenCallback callback) = 0;
  virtual void ValidateToken(const std::string& app_id,
                             const std::string& authorized_entity,
                             const std::string& scope,
                             const std::string& token,
                             ValidateTokenCallback callback) = 0;
  virtual void DeleteToken(const std::string& app_id,
                           const std::string& authorized_entity,
                           const std::string& scope,
                           DeleteTokenCallback callback) = 0;
  void DeleteAllTokensForApp(const std::string& app_id,
                             DeleteTokenCallback callback);

  // Persistence support.
  virtual void AddInstanceIDData(const std::string& app_id,
                                 const std::string& instance_id,
                                 const std::string& extra_data) = 0;
  virtual void RemoveInstanceIDData(const std::string& app_id) = 0;
  virtual void GetInstanceIDData(const std::string& app_id,
                                 GetInstanceIDDataCallback callback) = 0;
};

// Bridge between GCM users in Chrome and the platform-specific implementation.
// Obtain instances of this object by using |GCMProfileServiceFactory|.
class GCMDriver {
 public:
  // Max number of sender IDs that can be passed to |Register| on desktop.
  constexpr static size_t kMaxSenders = 100;

  using GCMAppHandlerMap =
      std::map<std::string, raw_ptr<GCMAppHandler, CtnExperimental>>;
  using RegisterCallback =
      base::OnceCallback<void(const std::string& registration_id,
                              GCMClient::Result result)>;
  using ValidateRegistrationCallback = base::OnceCallback<void(bool is_valid)>;
  using UnregisterCallback = base::OnceCallback<void(GCMClient::Result result)>;
  using SendCallback = base::OnceCallback<void(const std::string& message_id,
                                               GCMClient::Result result)>;
  using GetEncryptionInfoCallback =
      base::OnceCallback<void(std::string p256dh, std::string auth_secret)>;
  using EncryptMessageCallback =
      base::OnceCallback<void(GCMEncryptionResult result, std::string message)>;
  using DecryptMessageCallback =
      base::OnceCallback<void(GCMDecryptionResult result, std::string message)>;

  using GetGCMStatisticsCallback =
      base::OnceCallback<void(const GCMClient::GCMStatistics& stats)>;
  using GCMStatisticsRecordingCallback =
      base::RepeatingCallback<void(const GCMClient::GCMStatistics& stats)>;

  // Enumeration to be used with GetGCMStatistics() for indicating whether the
  // existing logs should be cleared or kept.
  enum ClearActivityLogs { CLEAR_LOGS, KEEP_LOGS };

  GCMDriver(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  GCMDriver(const GCMDriver&) = delete;
  GCMDriver& operator=(const GCMDriver&) = delete;

  virtual ~GCMDriver();

  // Registers |sender_ids| for an app. *Use |InstanceID| instead in new code.*
  //
  // A registration ID will be returned by the GCM server. On Android, only a
  // single sender ID is supported, but instead multiple simultaneous
  // registrations are allowed.
  // |app_id|: application ID.
  // |sender_ids|: list of IDs of the servers allowed to send messages to the
  //               application. The IDs are assigned by the Google API Console.
  //               Max number of IDs is 1 on Android, |kMaxSenders| on desktop.
  // |callback|: to be called once the asynchronous operation is done.
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                RegisterCallback callback);

  // Checks that the provided |sender_ids| and |registration_id| matches the
  // stored registration info for |app_id|.
  virtual void ValidateRegistration(const std::string& app_id,
                                    const std::vector<std::string>& sender_ids,
                                    const std::string& registration_id,
                                    ValidateRegistrationCallback callback) = 0;

  // Unregisters all sender_ids for an app. Only works on non-Android. Will also
  // remove any encryption keys associated with the |app_id|.
  // |app_id|: application ID.
  // |callback|: to be called once the asynchronous operation is done.
  void Unregister(const std::string& app_id, UnregisterCallback callback);

  // Unregisters an (app_id, sender_id) pair from using GCM. Only works on
  // Android. Will also remove any encryption keys associated with the |app_id|.
  // TODO(jianli): Switch to using GCM's unsubscribe API.
  // |app_id|: application ID.
  // |sender_id|: the sender ID that was passed when registering.
  // |callback|: to be called once the asynchronous operation is done.
  void UnregisterWithSenderId(const std::string& app_id,
                              const std::string& sender_id,
                              UnregisterCallback callback);

  // Sends a message to a given receiver.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  // |callback|: to be called once the asynchronous operation is done.
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message,
            SendCallback callback);

  // Get the public encryption key and the authentication secret associated with
  // |app_id|. If none have been associated with |app_id| yet, they will be
  // created. The |callback| will be invoked when it is available. Only use with
  // GCM registrations; use InstanceID::GetEncryptionInfo for InstanceID tokens.
  virtual void GetEncryptionInfo(const std::string& app_id,
                                 GetEncryptionInfoCallback callback);

  // Attempts to encrypt the |message| using draft-ietf-webpush-encryption-08
  // scheme using keys from internal key store. Either GetEncryptionInfo or
  // InstanceID::GetEncryptionInfo must be called once for keys to be available.
  // |callback| will be called asynchronously when |message| has been encrypted.
  // A dispatchable message will be used in case of success, an empty message in
  // case of failure.
  virtual void EncryptMessage(const std::string& app_id,
                              const std::string& authorized_entity,
                              const std::string& p256dh,
                              const std::string& auth_secret,
                              const std::string& message,
                              EncryptMessageCallback callback);

  // Attempts to decrypt the |message|using draft-ietf-webpush-encryption-08
  // scheme using keys from internal key store. Either GetEncryptionInfo or
  // InstanceID::GetEncryptionInfo must be called once for keys to be available.
  // |callback| will be called asynchronously when |message| has been decrypted.
  // A dispatchable message will be used in case of success, an empty message in
  // case of failure.
  // TODO(crbug.com/40116239): Decouple this from GCMDriver.
  virtual void DecryptMessage(const std::string& app_id,
                              const std::string& authorized_entity,
                              const std::string& message,
                              DecryptMessageCallback callback);

  const GCMAppHandlerMap& app_handlers() const { return app_handlers_; }

  // This method must be called before destroying the GCMDriver. Once it has
  // been called, no other GCMDriver methods may be used.
  virtual void Shutdown();

  // Adds a handler for a given app.
  virtual void AddAppHandler(const std::string& app_id, GCMAppHandler* handler);

  // Remove the handler for a given app.
  virtual void RemoveAppHandler(const std::string& app_id);

  // Returns the handler for the given app. May return a nullptr when no handler
  // could be found for the |app_id|.
  GCMAppHandler* GetAppHandler(const std::string& app_id);

  // Adds a connection state observer.
  virtual void AddConnectionObserver(GCMConnectionObserver* observer) = 0;

  // Removes a connection state observer.
  virtual void RemoveConnectionObserver(GCMConnectionObserver* observer) = 0;

  // For testing purpose. Always NULL on Android.
  virtual GCMClient* GetGCMClientForTesting() const = 0;

  // Returns true if the service was started.
  virtual bool IsStarted() const = 0;

  // Returns true if the gcm client has an open and active connection.
  virtual bool IsConnected() const = 0;

  // Get GCM client internal states and statistics. The activity logs will be
  // cleared before returning the stats when |clear_logs| is set to CLEAR_LOGS.
  virtual void GetGCMStatistics(GetGCMStatisticsCallback callback,
                                ClearActivityLogs clear_logs) = 0;

  // Enables/disables GCM activity recording, and then returns the stats.
  // |callback| will be called for new activity.
  virtual void SetGCMRecording(const GCMStatisticsRecordingCallback& callback,
                               bool recording) = 0;

  // sets a list of signed in accounts with OAuth2 access tokens, when GCMDriver
  // works in context of a signed in entity (e.g. browser profile where user is
  // signed into sync).
  // |account_tokens|: list of email addresses, account IDs and OAuth2 access
  //                   tokens.
  virtual void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) = 0;

  // Updates the |account_mapping| information in persistent store.
  virtual void UpdateAccountMapping(const AccountMapping& account_mapping) = 0;

  // Removes the account mapping information reated to |account_id| from
  // persistent store.
  virtual void RemoveAccountMapping(const CoreAccountId& account_id) = 0;

  // Getter and setter of last token fetch time.
  virtual base::Time GetLastTokenFetchTime() = 0;
  virtual void SetLastTokenFetchTime(const base::Time& time) = 0;

  // These methods must only be used by the InstanceID system.
  // The InstanceIDHandler provides an implementation for the InstanceID system.
  virtual InstanceIDHandler* GetInstanceIDHandlerInternal() = 0;
  // Allows the InstanceID system to integrate with GCM encryption storage.
  virtual GCMEncryptionProvider* GetEncryptionProviderInternal();

  // Adds or removes a custom client requested heartbeat interval. If multiple
  // components set that setting, the lowest setting will be used. If the
  // setting is outside of GetMax/MinClientHeartbeatIntervalMs() it will be
  // ignored. If a new setting is less than the currently used, the connection
  // will be reset with the new heartbeat. Client that no longer require
  // aggressive heartbeats, should remove their requested interval. Heartbeats
  // set this way survive connection/Chrome restart.
  //
  // GCM Driver can decide to postpone the action until Client is properly
  // initialized, hence this setting can be called at any time.
  //
  // Server can overwrite the setting to a different value.
  //
  // |scope| is used to identify the component that requests a custom interval
  // to be set, and allows that component to later revoke the setting.
  // |interval_ms| should be between 2 minues and 15 minues (28 minues on
  // cellular networks). For details check
  // GetMin/MaxClientHeartbeatItnervalMs() in HeartbeatManager.
  virtual void AddHeartbeatInterval(const std::string& scope,
                                    int interval_ms) = 0;
  virtual void RemoveHeartbeatInterval(const std::string& scope) = 0;

 protected:
  // Ensures that the GCM service starts (if necessary conditions are met).
  virtual GCMClient::Result EnsureStarted(GCMClient::StartMode start_mode) = 0;

  // Platform-specific implementation of Register.
  virtual void RegisterImpl(const std::string& app_id,
                            const std::vector<std::string>& sender_ids) = 0;

  // Platform-specific implementation of Unregister.
  virtual void UnregisterImpl(const std::string& app_id) = 0;

  // Platform-specific implementation of UnregisterWithSenderId.
  virtual void UnregisterWithSenderIdImpl(const std::string& app_id,
                                          const std::string& sender_id);

  // Platform-specific implementation of Send.
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const OutgoingMessage& message) = 0;

  // Platform-specific implementation of recording message decryption failures.
  virtual void RecordDecryptionFailure(const std::string& app_id,
                                       GCMDecryptionResult result) = 0;

  // Runs the Register callback.
  void RegisterFinished(const std::string& app_id,
                        const std::string& registration_id,
                        GCMClient::Result result);

  // To be called when a registration for |app_id| has been unregistered, having
  // |result| as the result of the unregistration. Will remove any encryption
  // information associated with the |app_id| and then calls UnregisterFinished.
  void RemoveEncryptionInfoAfterUnregister(const std::string& app_id,
                                           GCMClient::Result result);

  // Runs the Unregister callback.
  void UnregisterFinished(const std::string& app_id, GCMClient::Result result);

  // Runs the Send callback.
  void SendFinished(const std::string& app_id,
                    const std::string& message_id,
                    GCMClient::Result result);

  bool HasRegisterCallback(const std::string& app_id);

  void ClearCallbacks();

  // Dispatches the OnMessage event to the app handler associated with |app_id|.
  // If |message| has been encrypted, it will be decrypted asynchronously and
  // dispatched when the decryption operation was successful. Otherwise, the
  // |message| will be dispatched immediately to the handler for |app_id|.
  void DispatchMessage(const std::string& app_id,
                       const IncomingMessage& message);

 private:
  // Common code shared by Unregister and UnregisterWithSenderId.
  void UnregisterInternal(const std::string& app_id,
                          const std::string* sender_id,
                          UnregisterCallback callback);

  // Dispatches the OnMessage event to the app handler associated with |app_id|
  // if |result| indicates that it is safe to do so, or will report a decryption
  // failure for the |app_id| otherwise.
  void DispatchMessageInternal(const std::string& app_id,
                               GCMDecryptionResult result,
                               IncomingMessage message);

  // Called after unregistration completes in order to trigger the pending
  // registration.
  void RegisterAfterUnregister(
      const std::string& app_id,
      const std::vector<std::string>& normalized_sender_ids,
      UnregisterCallback unregister_callback,
      GCMClient::Result result);

  void OnMessageEncrypted(EncryptMessageCallback callback,
                          GCMEncryptionResult result,
                          std::string message);

  void OnMessageDecrypted(DecryptMessageCallback callback,
                          GCMDecryptionResult result,
                          IncomingMessage message);

  // Callback map (from app_id to callback) for Register.
  std::map<std::string, RegisterCallback> register_callbacks_;

  // Callback map (from app_id to callback) for Unregister.
  std::map<std::string, UnregisterCallback> unregister_callbacks_;

  // Callback map (from <app_id, message_id> to callback) for Send.
  std::map<std::pair<std::string, std::string>, SendCallback> send_callbacks_;

  // The encryption provider, used for key management and decryption of
  // encrypted, incoming messages.
  GCMEncryptionProvider encryption_provider_;

  // App handler map (from app_id to handler pointer). The handler is not owned.
  GCMAppHandlerMap app_handlers_;

  base::WeakPtrFactory<GCMDriver> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
