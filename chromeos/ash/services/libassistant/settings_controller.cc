// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/settings_controller.h"

#include <algorithm>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/ash/services/libassistant/callback_utils.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/utils/settings_utils.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/assistant/display_connection.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/settings_ui.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash::libassistant {

namespace {

// Each authentication token exists of a [gaia_id, access_token] tuple.
using AuthTokens = std::vector<std::pair<std::string, std::string>>;

AuthTokens ToAuthTokens(
    const std::vector<mojom::AuthenticationTokenPtr>& mojo_tokens) {
  AuthTokens result;

  for (const auto& token : mojo_tokens)
    result.emplace_back(token->gaia_id, token->access_token);

  return result;
}

const char* LocaleOrDefault(const std::string& locale) {
  if (locale.empty()) {
    // When |locale| is not provided we fall back to approximate locale.
    return icu::Locale::getDefault().getName();
  } else {
    return locale.c_str();
  }
}

}  // namespace

// Will be created as Libassistant is started, and will update the device
// settings when |UpdateSettings| is called.
// The device settings can not be updated earlier, as they require the
// device-id that is only assigned by Libassistant when it starts.
class SettingsController::DeviceSettingsUpdater
    : public AssistantClientObserver {
 public:
  DeviceSettingsUpdater(SettingsController* parent,
                        AssistantClient* assistant_client)
      : parent_(*parent), assistant_client_(*assistant_client) {}
  DeviceSettingsUpdater(const DeviceSettingsUpdater&) = delete;
  DeviceSettingsUpdater& operator=(const DeviceSettingsUpdater&) = delete;
  ~DeviceSettingsUpdater() override = default;

  void UpdateSettings(const std::string& locale, bool hotword_enabled) {
    const std::string device_id = assistant_client_->GetDeviceId();
    if (device_id.empty())
      return;

    // Update device id and device type.
    assistant::SettingsUiUpdate update;
    assistant::AssistantDeviceSettingsUpdate* device_settings_update =
        update.mutable_assistant_device_settings_update()
            ->add_assistant_device_settings_update();
    device_settings_update->set_device_id(device_id);
    device_settings_update->set_assistant_device_type(
        assistant::AssistantDevice::CROS);

    if (hotword_enabled) {
      device_settings_update->mutable_device_settings()->set_speaker_id_enabled(
          true);
    }

    VLOG(1) << "Assistant: Update device locale: " << locale;
    device_settings_update->mutable_device_settings()->set_locale(locale);

    // Enable personal readout to grant permission for personal features.
    device_settings_update->mutable_device_settings()->set_personal_readout(
        assistant::AssistantDeviceSettings::PERSONAL_READOUT_ENABLED);

    // Device settings update result is not handled because it is not included
    // in the SettingsUiUpdateResult.
    parent_->UpdateSettings(update.SerializeAsString(), base::DoNothing());
  }

  const raw_ref<SettingsController> parent_;
  const raw_ref<AssistantClient> assistant_client_;
};

// Sends a 'get settings' requests to Libassistant,
// waits for the response and forwards it to the callback.
// Will ensure the callback is always called, even when Libassistant is not
// running or stopped.
class GetSettingsResponseWaiter : public AbortableTask {
 public:
  GetSettingsResponseWaiter(SettingsController::GetSettingsCallback callback,
                            bool include_header)
      : callback_(std::move(callback)), include_header_(include_header) {}

  GetSettingsResponseWaiter(const GetSettingsResponseWaiter&) = delete;
  GetSettingsResponseWaiter& operator=(const GetSettingsResponseWaiter&) =
      delete;
  ~GetSettingsResponseWaiter() override { DCHECK(!callback_); }

  void SendRequest(AssistantClient* assistant_client,
                   const std::string& selector) {
    if (!assistant_client) {
      VLOG(1) << "Assistant: 'get settings' request while Libassistant is not "
                 "running.";
      Abort();
      return;
    }

    ::assistant::ui::SettingsUiSelector selector_proto;
    selector_proto.ParseFromString(selector);
    assistant_client->GetAssistantSettings(
        selector_proto, /*user_id=*/std::string(),
        base::BindOnce(&GetSettingsResponseWaiter::OnResponse,
                       weak_factory_.GetWeakPtr()));
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override {
    VLOG(1) << "Assistant: Aborting 'get settings' request";
    std::move(callback_).Run(std::string());
  }

 private:
  void OnResponse(
      const ::assistant::api::GetAssistantSettingsResponse& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // |settings_ui| is either a serialized proto message of |SettingsUi|
    // or |GetSettingsUiResponse| upon success, or an empty string otherwise.
    std::string settings_ui =
        UnwrapGetAssistantSettingsResponse(response, include_header_);
    std::move(callback_).Run(settings_ui);
  }

  // Ensures all callbacks are called on the current sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  SettingsController::GetSettingsCallback callback_;
  // Whether to include header in response. If this is true, a serialized proto
  // of GetSettingsUiResponse is passed to the callback; otherwise, a serialized
  // proto of SettingsUi is passed to the callback.
  bool include_header_;
  base::WeakPtrFactory<GetSettingsResponseWaiter> weak_factory_{this};
};

// Sends a 'update settings' requests to Libassistant,
// waits for the response and forwards it to the callback.
// Will ensure the callback is always called, even when Libassistant is not
// running or stopped.
class UpdateSettingsResponseWaiter : public AbortableTask {
 public:
  explicit UpdateSettingsResponseWaiter(
      SettingsController::UpdateSettingsCallback callback)
      : callback_(std::move(callback)) {}

  UpdateSettingsResponseWaiter(const UpdateSettingsResponseWaiter&) = delete;
  UpdateSettingsResponseWaiter& operator=(const UpdateSettingsResponseWaiter&) =
      delete;
  ~UpdateSettingsResponseWaiter() override = default;

  void SendRequest(AssistantClient* assistant_client,
                   const std::string& settings) {
    if (!assistant_client) {
      VLOG(1) << "Assistant: 'update settings' request while Libassistant is "
                 "not running.";
      Abort();
      return;
    }

    ::assistant::ui::SettingsUiUpdate update;
    update.ParseFromString(settings);
    assistant_client->UpdateAssistantSettings(
        update, /*user_id=*/std::string(),
        base::BindOnce(&UpdateSettingsResponseWaiter::OnResponse,
                       weak_factory_.GetWeakPtr()));
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override { std::move(callback_).Run(std::string()); }

 private:
  void OnResponse(
      const ::assistant::api::UpdateAssistantSettingsResponse& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // |update_result| is either a serialized proto message of
    // |SettingsUiUpdateResult| or an empty string.
    std::string update_result = UnwrapUpdateAssistantSettingsResponse(response);
    std::move(callback_).Run(update_result);
  }

  // Ensures all callbacks are called on the current sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  SettingsController::UpdateSettingsCallback callback_;
  base::WeakPtrFactory<UpdateSettingsResponseWaiter> weak_factory_{this};
};

SettingsController::SettingsController() = default;
SettingsController::~SettingsController() = default;

void SettingsController::Bind(
    mojo::PendingReceiver<mojom::SettingsController> receiver) {
  receiver_.Bind(std::move(receiver));
}

void SettingsController::SetAuthenticationTokens(
    std::vector<mojom::AuthenticationTokenPtr> tokens) {
  authentication_tokens_ = std::move(tokens);

  UpdateAuthenticationTokens(authentication_tokens_);
}

void SettingsController::SetLocale(const std::string& value) {
  locale_ = LocaleOrDefault(value);
  UpdateLocaleOverride(locale_);
  UpdateInternalOptions(locale_, spoken_feedback_enabled_, dark_mode_enabled_);
  UpdateDeviceSettings(locale_, hotword_enabled_);
}

void SettingsController::SetListeningEnabled(bool value) {
  listening_enabled_ = value;
  UpdateListeningEnabled(listening_enabled_);
}

void SettingsController::SetSpokenFeedbackEnabled(bool value) {
  spoken_feedback_enabled_ = value;
  UpdateInternalOptions(locale_, spoken_feedback_enabled_, dark_mode_enabled_);
}

void SettingsController::SetDarkModeEnabled(bool value) {
  dark_mode_enabled_ = value;
  UpdateDarkModeEnabledV2(dark_mode_enabled_);
}

void SettingsController::SetHotwordEnabled(bool value) {
  hotword_enabled_ = value;
  UpdateDeviceSettings(locale_, hotword_enabled_);
}

void SettingsController::GetSettings(const std::string& selector,
                                     bool include_header,
                                     GetSettingsCallback callback) {
  auto* waiter =
      pending_response_waiters_.Add(std::make_unique<GetSettingsResponseWaiter>(
          std::move(callback), include_header));
  waiter->SendRequest(assistant_client_, selector);
}

void SettingsController::UpdateSettings(const std::string& settings,
                                        UpdateSettingsCallback callback) {
  auto* waiter = pending_response_waiters_.Add(
      std::make_unique<UpdateSettingsResponseWaiter>(std::move(callback)));
  waiter->SendRequest(assistant_client_, settings);
}

void SettingsController::UpdateListeningEnabled(
    std::optional<bool> listening_enabled) {
  if (!assistant_client_)
    return;
  if (!listening_enabled.has_value())
    return;

  assistant_client_->EnableListening(listening_enabled.value());
}

void SettingsController::UpdateAuthenticationTokens(
    const std::optional<std::vector<mojom::AuthenticationTokenPtr>>& tokens) {
  if (!assistant_client_)
    return;
  if (!tokens.has_value())
    return;

  assistant_client_->SetAuthenticationInfo(ToAuthTokens(tokens.value()));
}

void SettingsController::UpdateInternalOptions(
    const std::optional<std::string>& locale,
    std::optional<bool> spoken_feedback_enabled,
    std::optional<bool> dark_mode_enabled) {
  if (!assistant_client_)
    return;

  if (locale.has_value() && spoken_feedback_enabled.has_value()) {
    assistant_client_->SetInternalOptions(locale.value(),
                                          spoken_feedback_enabled.value());
  }
}

void SettingsController::UpdateLocaleOverride(
    const std::optional<std::string>& locale) {
  if (!assistant_client_)
    return;

  if (!locale.has_value())
    return;

  assistant_client_->SetLocaleOverride(locale.value());
}

void SettingsController::UpdateDeviceSettings(
    const std::optional<std::string>& locale,
    std::optional<bool> hotword_enabled) {
  if (!device_settings_updater_)
    return;

  if (locale.has_value() && hotword_enabled.has_value()) {
    device_settings_updater_->UpdateSettings(locale.value(),
                                             hotword_enabled.value());
  }
}

void SettingsController::UpdateDarkModeEnabledV2(
    std::optional<bool> dark_mode_enabled) {
  if (!assistant_client_)
    return;

  if (!dark_mode_enabled.has_value())
    return;

  ::assistant::display::DisplayRequest display_request;
  display_request.mutable_set_device_properties_request()
      ->mutable_theme_properties_to_merge()
      ->set_mode(dark_mode_enabled.value()
                     ? ::assistant::api::params::ThemeProperties::DARK_THEME
                     : ::assistant::api::params::ThemeProperties::LIGHT_THEME);

  ::assistant::api::OnDisplayRequestRequest request;
  request.set_display_request_bytes(display_request.SerializeAsString());
  assistant_client_->SendDisplayRequest(request);
}

void SettingsController::OnAssistantClientCreated(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;

  // Note we do not enable the device settings updater here, as it requires
  // Libassistant to be fully ready.
  UpdateAuthenticationTokens(authentication_tokens_);
  UpdateInternalOptions(locale_, spoken_feedback_enabled_, dark_mode_enabled_);
}

void SettingsController::OnAssistantClientRunning(
    AssistantClient* assistant_client) {
  device_settings_updater_ =
      std::make_unique<DeviceSettingsUpdater>(this, assistant_client);

  UpdateDeviceSettings(locale_, hotword_enabled_);
  UpdateLocaleOverride(locale_);
  UpdateListeningEnabled(listening_enabled_);
  UpdateDarkModeEnabledV2(dark_mode_enabled_);
}

void SettingsController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  assistant_client_ = nullptr;
  device_settings_updater_ = nullptr;
  pending_response_waiters_.AbortAll();

  authentication_tokens_.reset();
  hotword_enabled_.reset();
  locale_.reset();
  spoken_feedback_enabled_.reset();
}

}  // namespace ash::libassistant
