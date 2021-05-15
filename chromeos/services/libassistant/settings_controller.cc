// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/settings_controller.h"

#include <algorithm>
#include <memory>

#include "base/callback_helpers.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/libassistant/callback_utils.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace chromeos {
namespace libassistant {

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

assistant_client::InternalOptions* WARN_UNUSED_RESULT CreateInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& locale,
    bool spoken_feedback_enabled) {
  auto* result = assistant_manager_internal->CreateDefaultInternalOptions();
  assistant::SetAssistantOptions(result, locale, spoken_feedback_enabled);

  result->SetClientControlEnabled(assistant::features::IsRoutinesEnabled());

  if (!assistant::features::IsVoiceMatchDisabled())
    result->EnableRequireVoiceMatchVerification();

  return result;
}

}  // namespace

// Will be created as Libassistant is started, and will update the device
// settings when |UpdateSettings| is called.
// The device settings can not be updated earlier, as they require the
// device-id that is only assigned by Libassistant when it starts.
class SettingsController::DeviceSettingsUpdater
    : public AssistantManagerObserver {
 public:
  DeviceSettingsUpdater(SettingsController* parent,
                        assistant_client::AssistantManager* assistant_manager)
      : parent_(*parent), assistant_manager_(*assistant_manager) {}
  DeviceSettingsUpdater(const DeviceSettingsUpdater&) = delete;
  DeviceSettingsUpdater& operator=(const DeviceSettingsUpdater&) = delete;
  ~DeviceSettingsUpdater() override = default;

  void UpdateSettings(const std::string& locale, bool hotword_enabled) {
    const std::string device_id = assistant_manager_.GetDeviceId();
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
    parent_.UpdateSettings(update.SerializeAsString(), base::DoNothing());
  }

  SettingsController& parent_;
  assistant_client::AssistantManager& assistant_manager_;
};

// Sends a 'get settings' requests to Libassistant,
// waits for the response and forwards it to the callback.
// Will ensure the callback is always called, even when Libassistant is not
// running or stopped.
class GetSettingsResponseWaiter : public AbortableTask {
 public:
  explicit GetSettingsResponseWaiter(
      SettingsController::GetSettingsCallback callback)
      : callback_(std::move(callback)) {}

  GetSettingsResponseWaiter(const GetSettingsResponseWaiter&) = delete;
  GetSettingsResponseWaiter& operator=(const GetSettingsResponseWaiter&) =
      delete;
  ~GetSettingsResponseWaiter() override { DCHECK(!callback_); }

  void SendRequest(
      assistant_client::AssistantManagerInternal* assistant_manager_internal,
      const std::string& selector) {
    if (!assistant_manager_internal) {
      VLOG(1) << "Assistant: 'get settings' request while Libassistant is not "
                 "running.";
      Abort();
      return;
    }

    std::string serialized_proto =
        assistant::SerializeGetSettingsUiRequest(selector);
    assistant_manager_internal->SendGetSettingsUiRequest(
        serialized_proto, /*user_id=*/std::string(),
        ToStdFunction(BindToCurrentSequence(
            base::BindOnce(&GetSettingsResponseWaiter::OnResponse,
                           weak_factory_.GetWeakPtr()))));
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override {
    VLOG(1) << "Assistant: Aborting 'get settings' request";
    std::move(callback_).Run(std::string());
  }

 private:
  void OnResponse(const assistant_client::VoicelessResponse& response) {
    std::string result = assistant::UnwrapGetSettingsUiResponse(response);
    std::move(callback_).Run(result);
  }

  SettingsController::GetSettingsCallback callback_;
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

  void SendRequest(
      assistant_client::AssistantManagerInternal* assistant_manager_internal,
      const std::string& settings) {
    if (!assistant_manager_internal) {
      VLOG(1) << "Assistant: 'update settings' request while Libassistant is "
                 "not running.";
      Abort();
      return;
    }

    std::string serialized_proto =
        assistant::SerializeUpdateSettingsUiRequest(settings);
    assistant_manager_internal->SendUpdateSettingsUiRequest(
        serialized_proto, /*user_id=*/std::string(),
        ToStdFunction(BindToCurrentSequence(
            base::BindOnce(&UpdateSettingsResponseWaiter::OnResponse,
                           weak_factory_.GetWeakPtr()))));
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override { std::move(callback_).Run(std::string()); }

 private:
  void OnResponse(const assistant_client::VoicelessResponse& response) {
    std::string result = assistant::UnwrapUpdateSettingsUiResponse(response);
    std::move(callback_).Run(result);
  }

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
  UpdateInternalOptions(locale_, spoken_feedback_enabled_);
  UpdateDeviceSettings(locale_, hotword_enabled_);
}

void SettingsController::SetListeningEnabled(bool value) {
  listening_enabled_ = value;
  UpdateListeningEnabled(listening_enabled_);
}

void SettingsController::SetSpokenFeedbackEnabled(bool value) {
  spoken_feedback_enabled_ = value;
  UpdateInternalOptions(locale_, spoken_feedback_enabled_);
}

void SettingsController::SetHotwordEnabled(bool value) {
  hotword_enabled_ = value;
  UpdateDeviceSettings(locale_, hotword_enabled_);
}

void SettingsController::GetSettings(const std::string& selector,
                                     GetSettingsCallback callback) {
  auto* waiter = pending_response_waiters_.Add(
      std::make_unique<GetSettingsResponseWaiter>(std::move(callback)));
  waiter->SendRequest(assistant_manager_internal_, selector);
}

void SettingsController::UpdateSettings(const std::string& settings,
                                        UpdateSettingsCallback callback) {
  auto* waiter = pending_response_waiters_.Add(
      std::make_unique<UpdateSettingsResponseWaiter>(std::move(callback)));
  waiter->SendRequest(assistant_manager_internal_, settings);
}

void SettingsController::UpdateListeningEnabled(
    absl::optional<bool> listening_enabled) {
  if (!assistant_manager_)
    return;
  if (!listening_enabled.has_value())
    return;

  assistant_manager_->EnableListening(listening_enabled.value());
}

void SettingsController::UpdateAuthenticationTokens(
    const absl::optional<std::vector<mojom::AuthenticationTokenPtr>>& tokens) {
  if (!assistant_manager_)
    return;
  if (!tokens.has_value())
    return;

  assistant_manager_->SetAuthTokens(ToAuthTokens(tokens.value()));
}

void SettingsController::UpdateInternalOptions(
    const absl::optional<std::string>& locale,
    absl::optional<bool> spoken_feedback_enabled) {
  if (!assistant_manager_internal_)
    return;

  if (locale.has_value())
    assistant_manager_internal_->SetLocaleOverride(locale.value());

  if (locale.has_value() && spoken_feedback_enabled.has_value()) {
    assistant_manager_internal_->SetOptions(
        *CreateInternalOptions(assistant_manager_internal_, locale.value(),
                               spoken_feedback_enabled.value()),
        [](bool success) { DVLOG(2) << "set options: " << success; });
  }
  return;
}

void SettingsController::UpdateDeviceSettings(
    const absl::optional<std::string>& locale,
    absl::optional<bool> hotword_enabled) {
  if (!device_settings_updater_)
    return;

  if (locale.has_value() && hotword_enabled.has_value()) {
    device_settings_updater_->UpdateSettings(locale.value(),
                                             hotword_enabled.value());
  }
}

void SettingsController::OnAssistantManagerCreated(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_ = assistant_manager;
  assistant_manager_internal_ = assistant_manager_internal;

  // Note we do not enable the device settings updater here, as it requires
  // Libassistant to be started.
  UpdateAuthenticationTokens(authentication_tokens_);
  UpdateInternalOptions(locale_, spoken_feedback_enabled_);
  UpdateListeningEnabled(listening_enabled_);
}

void SettingsController::OnAssistantManagerStarted(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  device_settings_updater_ =
      std::make_unique<DeviceSettingsUpdater>(this, assistant_manager);

  UpdateDeviceSettings(locale_, hotword_enabled_);
}

void SettingsController::OnDestroyingAssistantManager(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_ = nullptr;
  assistant_manager_internal_ = nullptr;
  device_settings_updater_ = nullptr;
  pending_response_waiters_.AbortAll();

  authentication_tokens_.reset();
  hotword_enabled_.reset();
  locale_.reset();
  spoken_feedback_enabled_.reset();
}

}  // namespace libassistant
}  // namespace chromeos
