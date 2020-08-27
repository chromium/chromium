// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_device_settings_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/services/assistant/cros_platform_api.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/service_context.h"
#include "libassistant/shared/public/platform_audio_output.h"

namespace client_op = ::assistant::api::client_op;

namespace chromeos {
namespace assistant {

class Setting {
 public:
  Setting() = default;
  Setting(Setting&) = delete;
  Setting& operator=(Setting&) = delete;
  virtual ~Setting() = default;

  virtual const char* setting_id() const = 0;
  virtual void Modify(const client_op::ModifySettingArgs& request) = 0;
};

namespace {

constexpr char kWiFiDeviceSettingId[] = "WIFI";
constexpr char kBluetoothDeviceSettingId[] = "BLUETOOTH";
constexpr char kScreenBrightnessDeviceSettingId[] = "BRIGHTNESS_LEVEL";
constexpr char kDoNotDisturbDeviceSettingId[] = "DO_NOT_DISTURB";
constexpr char kNightLightDeviceSettingId[] = "NIGHT_LIGHT_SWITCH";
constexpr char kSwitchAccessDeviceSettingId[] = "SWITCH_ACCESS";

constexpr float kDefaultSliderStep = 0.1f;

void LogUnsupportedChange(client_op::ModifySettingArgs args) {
  LOG(ERROR) << "Unsupported change operation: " << args.change()
             << " for setting " << args.setting_id();
}

void HandleOnOffChange(client_op::ModifySettingArgs modify_setting_args,
                       std::function<void(bool)> on_off_handler) {
  switch (modify_setting_args.change()) {
    case client_op::ModifySettingArgs_Change_ON:
      on_off_handler(true);
      return;
    case client_op::ModifySettingArgs_Change_OFF:
      on_off_handler(false);
      return;

    // Currently there are no use-cases for toggling.  This could change in the
    // future.
    case client_op::ModifySettingArgs_Change_TOGGLE:
      break;

    case client_op::ModifySettingArgs_Change_SET:
    case client_op::ModifySettingArgs_Change_INCREASE:
    case client_op::ModifySettingArgs_Change_DECREASE:
    case client_op::ModifySettingArgs_Change_UNSPECIFIED:
      // This shouldn't happen.
      break;
  }
  LogUnsupportedChange(modify_setting_args);
}

// Helper function that converts a slider value sent from the server, either
// absolute or a delta, from a given unit (e.g., STEP), to a percentage.
double ConvertSliderValueToLevel(double value,
                                 client_op::ModifySettingArgs_Unit unit,
                                 double default_value) {
  switch (unit) {
    case client_op::ModifySettingArgs_Unit_RANGE:
      // "set brightness to 20%".
      return value;
    case client_op::ModifySettingArgs_Unit_STEP:
      // "set brightness to 20".  Treat the step as a percentage.
      return value / 100.0f;

    // Currently, factor (e.g., 'double the brightness') and decibel units
    // aren't handled by the backend.  This could change in the future.
    case client_op::ModifySettingArgs_Unit_FACTOR:
    case client_op::ModifySettingArgs_Unit_DECIBEL:
      break;

    case client_op::ModifySettingArgs_Unit_NATIVE:
    case client_op::ModifySettingArgs_Unit_UNKNOWN_UNIT:
      // This shouldn't happen.
      break;
  }
  LOG(ERROR) << "Unsupported slider unit: " << unit;
  return default_value;
}

void HandleSliderChange(client_op::ModifySettingArgs request,
                        std::function<void(double)> set_value_handler,
                        std::function<double()> get_value_handler) {
  switch (request.change()) {
    case client_op::ModifySettingArgs_Change_SET: {
      // For unsupported units, set the value to the current value, for
      // visual feedback.
      double new_value = ConvertSliderValueToLevel(
          request.numeric_value(), request.unit(), get_value_handler());
      set_value_handler(new_value);
      return;
    }

    case client_op::ModifySettingArgs_Change_INCREASE:
    case client_op::ModifySettingArgs_Change_DECREASE: {
      double current_value = get_value_handler();
      double step = kDefaultSliderStep;
      if (request.numeric_value() != 0.0f) {
        // For unsupported units, use the default step percentage.
        step = ConvertSliderValueToLevel(request.numeric_value(),
                                         request.unit(), kDefaultSliderStep);
      }
      double new_value =
          (request.change() == client_op::ModifySettingArgs_Change_INCREASE)
              ? std::min(current_value + step, 1.0)
              : std::max(current_value - step, 0.0);
      set_value_handler(new_value);
      return;
    }

    case client_op::ModifySettingArgs_Change_ON:
    case client_op::ModifySettingArgs_Change_OFF:
    case client_op::ModifySettingArgs_Change_TOGGLE:
    case client_op::ModifySettingArgs_Change_UNSPECIFIED:
      // This shouldn't happen.
      break;
  }
  LogUnsupportedChange(request);
}

class SettingWithDeviceAction : public Setting {
 public:
  explicit SettingWithDeviceAction(ServiceContext* context)
      : context_(context) {}

  DeviceActions* device_actions() { return context_->device_actions(); }

 private:
  ServiceContext* context_;
};

class WifiSetting : public SettingWithDeviceAction {
 public:
  using SettingWithDeviceAction::SettingWithDeviceAction;

  const char* setting_id() const override { return kWiFiDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      this->device_actions()->SetWifiEnabled(enabled);
    });
  }
};

class BluetoothSetting : public SettingWithDeviceAction {
 public:
  using SettingWithDeviceAction::SettingWithDeviceAction;

  const char* setting_id() const override { return kBluetoothDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      this->device_actions()->SetBluetoothEnabled(enabled);
    });
  }
};

class DoNotDisturbSetting : public Setting {
 public:
  explicit DoNotDisturbSetting(ServiceContext* context) : context_(context) {}

  const char* setting_id() const override {
    return kDoNotDisturbDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      this->assistant_notification_controller()->SetQuietMode(enabled);
    });
  }

 private:
  ash::AssistantNotificationController* assistant_notification_controller() {
    return context_->assistant_notification_controller();
  }

  ServiceContext* context_;
};

class SwitchAccessSetting : public SettingWithDeviceAction {
 public:
  using SettingWithDeviceAction::SettingWithDeviceAction;

  const char* setting_id() const override {
    return kSwitchAccessDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      this->device_actions()->SetSwitchAccessEnabled(enabled);
    });
  }
};

class NightLightSetting : public SettingWithDeviceAction {
 public:
  using SettingWithDeviceAction::SettingWithDeviceAction;

  const char* setting_id() const override { return kNightLightDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      this->device_actions()->SetNightLightEnabled(enabled);
    });
  }
};

class BrightnessSetting : public SettingWithDeviceAction {
 public:
  explicit BrightnessSetting(ServiceContext* context)
      : SettingWithDeviceAction(context), weak_factory_(this) {}

  const char* setting_id() const override {
    return kScreenBrightnessDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    device_actions()->GetScreenBrightnessLevel(base::BindOnce(
        [](base::WeakPtr<BrightnessSetting> this_,
           client_op::ModifySettingArgs request, bool success,
           double current_value) {
          if (!success || !this_) {
            LOG(WARNING) << "Failed to get brightness level";
            return;
          }
          HandleSliderChange(
              request,
              [&this_](double new_value) {
                this_->device_actions()->SetScreenBrightnessLevel(new_value,
                                                                  true);
              },
              [current_value]() { return current_value; });
        },
        weak_factory_.GetWeakPtr(), request));
  }

 private:
  base::WeakPtrFactory<BrightnessSetting> weak_factory_;
};

}  // namespace

AssistantDeviceSettingsDelegate::AssistantDeviceSettingsDelegate(
    ServiceContext* context) {
  AddSetting(std::make_unique<WifiSetting>(context));
  AddSetting(std::make_unique<BluetoothSetting>(context));
  AddSetting(std::make_unique<NightLightSetting>(context));
  AddSetting(std::make_unique<DoNotDisturbSetting>(context));
  AddSetting(std::make_unique<BrightnessSetting>(context));
  AddSetting(std::make_unique<SwitchAccessSetting>(context));
}

AssistantDeviceSettingsDelegate::~AssistantDeviceSettingsDelegate() = default;

bool AssistantDeviceSettingsDelegate::IsSettingSupported(
    const std::string& setting_id) const {
  return std::any_of(settings_.begin(), settings_.end(),
                     [&setting_id](const auto& setting) {
                       return setting->setting_id() == setting_id;
                     });
}

void AssistantDeviceSettingsDelegate::HandleModifyDeviceSetting(
    const client_op::ModifySettingArgs& modify_setting_args) {
  VLOG(1) << "Assistant: Modifying Device Setting '"
          << modify_setting_args.setting_id() << "'";
  DCHECK(IsSettingSupported(modify_setting_args.setting_id()));

  for (const auto& setting : settings_) {
    if (setting->setting_id() == modify_setting_args.setting_id()) {
      setting->Modify(modify_setting_args);
      return;
    }
  }

  NOTREACHED();
}

std::vector<DeviceSetting> AssistantDeviceSettingsDelegate::GetDeviceSettings(
    const ::assistant::api::client_op::GetDeviceSettingsArgs& args) const {
  std::vector<DeviceSetting> result;
  for (const std::string& setting_id : args.setting_ids())
    result.emplace_back(setting_id, IsSettingSupported(setting_id));
  return result;
}

void AssistantDeviceSettingsDelegate::AddSetting(
    std::unique_ptr<Setting> setting) {
  settings_.push_back(std::move(setting));
}

}  // namespace assistant
}  // namespace chromeos
