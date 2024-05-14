// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/device_settings_controller.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/public/mojom/device_settings_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/util.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/device_args.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/internal_options.pb.h"

namespace client_op = ::assistant::api::client_op;

namespace ash::libassistant {

using mojom::DeviceSettingsDelegate;
using mojom::GetBrightnessResultPtr;

namespace {
// A macro which ensures we are running on the main thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }
}  // namespace

class Setting {
 public:
  explicit Setting(DeviceSettingsDelegate* delegate) : delegate_(*delegate) {}
  Setting(Setting&) = delete;
  Setting& operator=(Setting&) = delete;
  virtual ~Setting() = default;

  virtual const char* setting_id() const = 0;
  virtual void Modify(const client_op::ModifySettingArgs& request) = 0;

  DeviceSettingsDelegate& delegate() { return *delegate_; }

 private:
  const raw_ref<DeviceSettingsDelegate> delegate_;
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

class WifiSetting : public Setting {
 public:
  using Setting::Setting;

  const char* setting_id() const override { return kWiFiDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(
        request, [&](bool enabled) { delegate().SetWifiEnabled(enabled); });
  }
};

class BluetoothSetting : public Setting {
 public:
  using Setting::Setting;

  const char* setting_id() const override { return kBluetoothDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      VLOG(1) << "Assistant: Setting bluetooth enabled: " << enabled;
      delegate().SetBluetoothEnabled(enabled);
    });
  }
};

class DoNotDisturbSetting : public Setting {
 public:
  using Setting::Setting;

  const char* setting_id() const override {
    return kDoNotDisturbDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      VLOG(1) << "Assistant: Setting do not disturb enabled: " << enabled;
      delegate().SetDoNotDisturbEnabled(enabled);
    });
  }
};

class SwitchAccessSetting : public Setting {
 public:
  using Setting::Setting;

  const char* setting_id() const override {
    return kSwitchAccessDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      VLOG(1) << "Assistant: Setting switch access enabled: " << enabled;
      delegate().SetSwitchAccessEnabled(enabled);
    });
  }
};

class NightLightSetting : public Setting {
 public:
  using Setting::Setting;

  const char* setting_id() const override { return kNightLightDeviceSettingId; }

  void Modify(const client_op::ModifySettingArgs& request) override {
    HandleOnOffChange(request, [&](bool enabled) {
      VLOG(1) << "Assistant: Setting night light enabled: " << enabled;
      delegate().SetNightLightEnabled(enabled);
    });
  }
};

class BrightnessSetting : public Setting {
 public:
  explicit BrightnessSetting(DeviceSettingsDelegate* delegate)
      : Setting(delegate), weak_factory_(this) {}

  const char* setting_id() const override {
    return kScreenBrightnessDeviceSettingId;
  }

  void Modify(const client_op::ModifySettingArgs& request) override {
    delegate().GetScreenBrightnessLevel(base::BindOnce(
        [](base::WeakPtr<BrightnessSetting> this_,
           client_op::ModifySettingArgs request,
           GetBrightnessResultPtr result) {
          if (!result || !this_) {
            LOG(WARNING) << "Failed to get brightness level";
            return;
          }
          HandleSliderChange(
              request,
              [&this_](double new_value) {
                VLOG(1) << "Assistant: Setting brightness to " << new_value
                        << " percent";
                this_->delegate().SetScreenBrightnessLevel(new_value, true);
              },
              [current_value = result->level]() { return current_value; });
        },
        weak_factory_.GetWeakPtr(), request));
  }

 private:
  base::WeakPtrFactory<BrightnessSetting> weak_factory_;
};

}  // namespace

DeviceSettingsController::DeviceSettingsController()
    : mojom_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
DeviceSettingsController::~DeviceSettingsController() = default;

void DeviceSettingsController::Bind(
    mojo::PendingRemote<mojom::DeviceSettingsDelegate> remote) {
  remote_.Bind(std::move(remote));

  AddSetting(std::make_unique<WifiSetting>(remote_.get()));
  AddSetting(std::make_unique<BluetoothSetting>(remote_.get()));
  AddSetting(std::make_unique<NightLightSetting>(remote_.get()));
  AddSetting(std::make_unique<DoNotDisturbSetting>(remote_.get()));
  AddSetting(std::make_unique<BrightnessSetting>(remote_.get()));
  AddSetting(std::make_unique<SwitchAccessSetting>(remote_.get()));
}

void DeviceSettingsController::OnModifyDeviceSetting(
    const client_op::ModifySettingArgs& modify_setting_args) {
  ENSURE_MOJOM_THREAD(&DeviceSettingsController::OnModifyDeviceSetting,
                      modify_setting_args);
  VLOG(1) << "Assistant: Modifying Device Setting '"
          << modify_setting_args.setting_id() << "'";
  DCHECK(IsSettingSupported(modify_setting_args.setting_id()));

  for (const auto& setting : settings_) {
    if (setting->setting_id() == modify_setting_args.setting_id()) {
      setting->Modify(modify_setting_args);
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

void DeviceSettingsController::OnGetDeviceSettings(
    int interaction_id,
    const ::assistant::api::client_op::GetDeviceSettingsArgs& args) {
  if (!assistant_client_) {
    VLOG(1) << "Assistant: Dropping OnGetDeviceSettings call as Libassistant "
               "has not started yet";
    return;
  }

  std::vector<chromeos::assistant::DeviceSetting> result =
      GetSupportedDeviceSettings(args);

  auto interaction_proto = ash::libassistant::CreateGetDeviceSettingInteraction(
      interaction_id, result);

  ::assistant::api::VoicelessOptions options;
  options.set_is_user_initiated(true);

  assistant_client_->SendVoicelessInteraction(
      interaction_proto, /*description=*/"get_settings_result", options,
      base::DoNothing());
}

void DeviceSettingsController::OnAssistantClientCreated(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
}

void DeviceSettingsController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  assistant_client_ = nullptr;
}

std::vector<chromeos::assistant::DeviceSetting>
DeviceSettingsController::GetSupportedDeviceSettings(
    const ::assistant::api::client_op::GetDeviceSettingsArgs& args) const {
  std::vector<chromeos::assistant::DeviceSetting> result;
  for (const std::string& setting_id : args.setting_ids())
    result.emplace_back(setting_id, IsSettingSupported(setting_id));
  return result;
}

bool DeviceSettingsController::IsSettingSupported(
    const std::string& setting_id) const {
  return base::Contains(settings_, setting_id, &Setting::setting_id);
}

void DeviceSettingsController::AddSetting(std::unique_ptr<Setting> setting) {
  settings_.push_back(std::move(setting));
}

}  // namespace ash::libassistant
