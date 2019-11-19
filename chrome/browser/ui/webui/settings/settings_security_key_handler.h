// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "device/fido/fido_constants.h"

#include "device/fido/bio/enrollment.h"

namespace base {
class ListValue;
}

namespace device {
struct AggregatedEnumerateCredentialsResponse;
class FidoDiscoveryFactory;
class CredentialManagementHandler;
enum class CredentialManagementStatus;
class SetPINRequestHandler;
class ResetRequestHandler;
class BioEnrollmentHandler;
enum class BioEnrollmentStatus;
}  // namespace device

namespace settings {

// Base class for message handlers on the "Security Keys" settings subpage.
class SecurityKeysHandlerBase : public SettingsPageUIHandler {
 protected:
  SecurityKeysHandlerBase() = default;

  // Subclasses must implement close to invalidate all pending callbacks.
  virtual void Close() = 0;

 private:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  SecurityKeysHandlerBase(const SecurityKeysHandlerBase&) = delete;
  SecurityKeysHandlerBase& operator=(const SecurityKeysHandlerBase&) = delete;
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

  void HandleStartSetPIN(const base::ListValue* args);
  void OnGatherPIN(base::Optional<int64_t> num_retries);
  void OnSetPINComplete(device::CtapDeviceResponseCode code);
  void HandleSetPIN(const base::ListValue* args);

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

  void HandleReset(const base::ListValue* args);
  void OnResetSent();
  void HandleCompleteReset(const base::ListValue* args);
  void OnResetFinished(device::CtapDeviceResponseCode result);

  State state_ = State::kNone;

  std::unique_ptr<device::ResetRequestHandler> reset_;
  base::Optional<device::CtapDeviceResponseCode> reset_result_;

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

 private:
  enum class State {
    kNone,
    kStart,
    kPIN,
    kReady,
    kGettingCredentials,
    kDeletingCredentials,
  };

  void RegisterMessages() override;
  void Close() override;

  void HandleStart(const base::ListValue* args);
  void HandlePIN(const base::ListValue* args);
  void HandleEnumerate(const base::ListValue* args);
  void HandleDelete(const base::ListValue* args);

  void OnCredentialManagementReady();
  void OnHaveCredentials(
      device::CtapDeviceResponseCode status,
      base::Optional<
          std::vector<device::AggregatedEnumerateCredentialsResponse>>
          responses,
      base::Optional<size_t> remaining_credentials);
  void OnGatherPIN(int64_t num_retries, base::OnceCallback<void(std::string)>);
  void OnCredentialsDeleted(device::CtapDeviceResponseCode status);
  void OnFinished(device::CredentialManagementStatus status);

  State state_ = State::kNone;
  base::OnceCallback<void(std::string)> credential_management_provide_pin_cb_;

  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;
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

  void HandleStart(const base::ListValue* args);
  void OnReady();
  void OnError(device::BioEnrollmentStatus status);
  void OnGatherPIN(int64_t retries, base::OnceCallback<void(std::string)>);

  void HandleProvidePIN(const base::ListValue* args);

  void HandleEnumerate(const base::ListValue* args);
  void OnHaveEnumeration(
      device::CtapDeviceResponseCode,
      base::Optional<std::map<std::vector<uint8_t>, std::string>>);

  void HandleStartEnrolling(const base::ListValue* args);
  void OnEnrollingResponse(device::BioEnrollmentSampleStatus, uint8_t);
  void OnEnrollmentFinished(device::CtapDeviceResponseCode,
                            std::vector<uint8_t> template_id);
  void OnHavePostEnrollmentEnumeration(
      std::vector<uint8_t> enrolled_template_id,
      device::CtapDeviceResponseCode code,
      base::Optional<std::map<std::vector<uint8_t>, std::string>> enrollments);

  void HandleDelete(const base::ListValue* args);
  void OnDelete(device::CtapDeviceResponseCode);

  void HandleRename(const base::ListValue* args);
  void OnRename(device::CtapDeviceResponseCode);

  void HandleCancel(const base::ListValue* args);

  State state_ = State::kNone;
  std::string callback_id_;
  base::OnceCallback<void(std::string)> provide_pin_cb_;
  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;
  std::unique_ptr<device::BioEnrollmentHandler> bio_;
  base::WeakPtrFactory<SecurityKeysBioEnrollmentHandler> weak_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
