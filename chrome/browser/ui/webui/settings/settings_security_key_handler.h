// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/bio/enrollment_handler.h"
#include "device/fido/credential_management_handler.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"

namespace device {
struct AggregatedEnumerateCredentialsResponse;
enum class CredentialManagementStatus;
class SetPINRequestHandler;
class ResetRequestHandler;
}  // namespace device

class LocalCredentialManagement;

namespace settings {

// Base class for message handlers on the "Security Keys" settings subpage.
class SecurityKeysHandlerBase : public SettingsPageUIHandler {
 public:
  SecurityKeysHandlerBase(const SecurityKeysHandlerBase&) = delete;
  SecurityKeysHandlerBase& operator=(const SecurityKeysHandlerBase&) = delete;

 protected:
  SecurityKeysHandlerBase();
  explicit SecurityKeysHandlerBase(
      std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory);
  ~SecurityKeysHandlerBase() override;

  // Subclasses must implement close to invalidate all pending callbacks.
  virtual void Close() = 0;

  // Returns the discovery factory to be used for the request.
  device::FidoDiscoveryFactory* discovery_factory() {
    return discovery_factory_.get();
  }

 private:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_ =
      std::make_unique<device::FidoDiscoveryFactory>();
};

// SecurityKeysPINHandler processes messages from the "Create a PIN" dialog of
// the "Security Keys" settings subpage. An instance of this class is created
// for each settings tab and is destroyed when the tab is closed. See
// SecurityKeysPINBrowserProxy about the interface.
class SecurityKeysPINHandler : public SecurityKeysHandlerBase {
 public:
  SecurityKeysPINHandler();
  ~SecurityKeysPINHandler() override;

 private:
  enum class State {
    kNone,
    kStartSetPIN,
    kGatherNewPIN,
    kGatherChangePIN,
    kSettingPIN,
  };

  void RegisterMessages() override;
  void Close() override;

  void HandleStartSetPIN(const base::Value::List& args);
  void OnGatherPIN(uint32_t current_min_pin_length,
                   uint32_t new_min_pin_length,
                   std::optional<int64_t> num_retries);
  void OnSetPINComplete(device::CtapDeviceResponseCode code);
  void HandleSetPIN(const base::Value::List& args);

  State state_ = State::kNone;

  std::unique_ptr<device::SetPINRequestHandler> set_pin_;

  std::string callback_id_;
  base::WeakPtrFactory<SecurityKeysPINHandler> weak_factory_{this};
};

// SecurityKeysResetHandler processes messages from the "Reset your Security
// Key" dialog of the "Security Keys" settings subpage. An instance of this
// class is created for each settings tab and is destroyed when the tab is
// closed. See SecurityKeysResetBrowserProxy about the interface.
class SecurityKeysResetHandler : public SecurityKeysHandlerBase {
 public:
  SecurityKeysResetHandler();
  ~SecurityKeysResetHandler() override;

 private:
  enum class State {
    kNone,
    kStartReset,
    kWaitingForResetNoCallbackYet,
    kWaitingForResetHaveCallback,
    kWaitingForCompleteReset,
  };

  void RegisterMessages() override;
  void Close() override;

  void HandleReset(const base::Value::List& args);
  void OnResetSent();
  void HandleCompleteReset(const base::Value::List& args);
  void OnResetFinished(device::CtapDeviceResponseCode result);

  State state_ = State::kNone;

  std::unique_ptr<device::ResetRequestHandler> reset_;
  std::optional<device::CtapDeviceResponseCode> reset_result_;

  std::string callback_id_;
  base::WeakPtrFactory<SecurityKeysResetHandler> weak_factory_{this};
};

// SecurityKeysCredentialHandler processes messages from the "Manage
// sign-in data" dialog of the "Security Keys" settings subpage. An instance of
// this class is created for each settings tab and is destroyed when the tab is
// closed. See SecurityKeysCredentialBrowserProxy about the interface.
class SecurityKeysCredentialHandler : public SecurityKeysHandlerBase {
 public:
  SecurityKeysCredentialHandler();
  ~SecurityKeysCredentialHandler() override;

 protected:
  explicit SecurityKeysCredentialHandler(
      std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory);
  void HandleStart(const base::Value::List& args);
  void HandlePIN(const base::Value::List& args);
  void HandleUpdateUserInformation(const base::Value::List& args);

 private:
  enum class State {
    kNone,
    kStart,
    kPIN,
    kReady,
    kGettingCredentials,
    kDeletingCredentials,
    kUpdatingUserInformation,
  };

  void RegisterMessages() override;
  void Close() override;

  void HandleEnumerate(const base::Value::List& args);
  void HandleDelete(const base::Value::List& args);

  void OnCredentialManagementReady();
  void OnHaveCredentials(
      device::CtapDeviceResponseCode status,
      std::optional<std::vector<device::AggregatedEnumerateCredentialsResponse>>
          responses,
      std::optional<size_t> remaining_credentials);
  void OnGatherPIN(device::CredentialManagementHandler::AuthenticatorProperties
                       authenticator_properties,
                   base::OnceCallback<void(std::string)>);
  void OnCredentialsDeleted(device::CtapDeviceResponseCode status);
  void OnUserInformationUpdated(device::CtapDeviceResponseCode status);
  void OnFinished(device::CredentialManagementStatus status);

  State state_ = State::kNone;
  base::OnceCallback<void(std::string)> credential_management_provide_pin_cb_;

  std::unique_ptr<device::CredentialManagementHandler> credential_management_;

  std::string callback_id_;
  base::WeakPtrFactory<SecurityKeysCredentialHandler> weak_factory_{this};
};

// SecurityKeysBioEnrollmentHandler processes messages from the "Manage
// fingerprints" dialog of the "Security Keys" settings subpage. An instance of
// this class is created for each settings tab and is destroyed when the tab is
// closed. See SecurityKeysBioEnrollProxy about the interface.
class SecurityKeysBioEnrollmentHandler : public SecurityKeysHandlerBase {
 public:
  SecurityKeysBioEnrollmentHandler();
  ~SecurityKeysBioEnrollmentHandler() override;

 protected:
  explicit SecurityKeysBioEnrollmentHandler(
      std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory);
  void HandleStart(const base::Value::List& args);
  void HandleProvidePIN(const base::Value::List& args);
  void HandleStartEnrolling(const base::Value::List& args);

 private:
  enum class State {
    kNone,
    kStart,
    kGatherPIN,
    kReady,
    kEnumerating,
    kEnrolling,
    kDeleting,
    kRenaming,
  };

  void RegisterMessages() override;
  void Close() override;

  void OnReady(device::BioEnrollmentHandler::SensorInfo sensor_info);
  void OnError(device::BioEnrollmentHandler::Error error);
  void OnGatherPIN(uint32_t min_pin_length,
                   int64_t num_retries,
                   base::OnceCallback<void(std::string)>);
  void HandleGetSensorInfo(const base::Value::List& args);

  void HandleEnumerate(const base::Value::List& args);
  void OnHaveEnumeration(
      device::CtapDeviceResponseCode,
      std::optional<std::map<std::vector<uint8_t>, std::string>>);

  void OnEnrollingResponse(device::BioEnrollmentSampleStatus, uint8_t);
  void OnEnrollmentFinished(device::CtapDeviceResponseCode,
                            std::vector<uint8_t> template_id);
  void OnHavePostEnrollmentEnumeration(
      std::vector<uint8_t> enrolled_template_id,
      device::CtapDeviceResponseCode code,
      std::optional<std::map<std::vector<uint8_t>, std::string>> enrollments);

  void HandleDelete(const base::Value::List& args);
  void OnDelete(device::CtapDeviceResponseCode);

  void HandleRename(const base::Value::List& args);
  void OnRename(device::CtapDeviceResponseCode);

  void HandleCancel(const base::Value::List& args);

  State state_ = State::kNone;
  std::string callback_id_;
  base::OnceCallback<void(std::string)> provide_pin_cb_;
  std::unique_ptr<device::BioEnrollmentHandler> bio_;
  device::BioEnrollmentHandler::SensorInfo sensor_info_;
  base::WeakPtrFactory<SecurityKeysBioEnrollmentHandler> weak_factory_{this};
};

class SecurityKeysPhonesHandler : public SettingsPageUIHandler {
 public:
  SecurityKeysPhonesHandler();
  ~SecurityKeysPhonesHandler() override;

 protected:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleEnumerate(const base::Value::List& args);
  void HandleDelete(const base::Value::List& args);
  void HandleRename(const base::Value::List& args);

  void DoEnumerate(const base::Value& callback_id);
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class PasskeysHandler : public SettingsPageUIHandler {
 public:
  PasskeysHandler();
  explicit PasskeysHandler(
      std::unique_ptr<LocalCredentialManagement> local_cred_man);
  ~PasskeysHandler() override;

 protected:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleEdit(const base::Value::List& args);
  void OnEditComplete(std::string callback_id, bool edit_ok);

  void HandleDelete(const base::Value::List& args);
  void OnDeleteComplete(std::string callback_id, bool delete_ok);

 private:
  void HandleHasPasskeys(const base::Value::List& args);
  void OnHasPasskeysComplete(std::string callback_id, bool has_passkeys);

  void HandleManagePasskeys(const base::Value::List& args);

  void HandleEnumerate(const base::Value::List& args);
  void DoEnumerate(std::string callback_id);
  void OnEnumerateComplete(
      std::string callback_id,
      std::optional<std::vector<device::DiscoverableCredentialMetadata>>
          credentials);

  std::unique_ptr<LocalCredentialManagement> local_cred_man_{nullptr};

  base::WeakPtrFactory<PasskeysHandler> weak_factory_{this};
};

#endif

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
