// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.mojom-forward.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/meta_key.mojom-shared.h"
#include "ui/views/widget/widget.h"

namespace ash::settings {

namespace {

using ActionTypeVariant =
    absl::variant<AcceleratorAction, ::ash::mojom::StaticShortcutAction>;

constexpr double kDefaultKeyboardBrightness = 40.0;

// Used to represent a constant version of the mojom::ActionChoice struct.
struct ActionChoice {
  int id;
  ActionTypeVariant action_variant;
};

constexpr ActionChoice kMouseButtonOptions[] = {
    {IDS_SETTINGS_VOLUME_ON_OFF_OPTION_LABEL,
     AcceleratorAction::kVolumeMuteToggle},
    {IDS_SETTINGS_MICROPHONE_ON_OFF_OPTION_LABEL,
     AcceleratorAction::kMicrophoneMuteToggle},
    {IDS_SETTINGS_MEDIA_PLAY_PAUSE_OPTION_LABEL,
     AcceleratorAction::kMediaPlayPause},
    {IDS_SETTINGS_OVERVIEW_OPTION_LABEL, AcceleratorAction::kToggleOverview},
    {IDS_SETTINGS_SCREENSHOT_OPTION_LABEL,
     AcceleratorAction::kTakePartialScreenshot},
    {IDS_SETTINGS_PREVIOUS_PAGE_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kPreviousPage},
    {IDS_SETTINGS_NEXT_PAGE_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kNextPage},
    {IDS_SETTINGS_EMOJI_PICKER_OPTION_LABEL,
     AcceleratorAction::kShowEmojiPicker},
    {IDS_SETTINGS_HIGH_CONTRAST_ON_OFF_OPTION_LABEL,
     AcceleratorAction::kToggleHighContrast},
    {IDS_SETTINGS_MAGNIFIER_ON_OFF_OPTION_LABEL,
     AcceleratorAction::kToggleFullscreenMagnifier},
    {IDS_SETTINGS_DICTATION_ON_OFF_OPTION_LABEL,
     AcceleratorAction::kEnableOrToggleDictation},
    {IDS_SETTINGS_LEFT_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kLeftClick},
    {IDS_SETTINGS_RIGHT_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kRightClick},
    {IDS_SETTINGS_MIDDLE_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kMiddleClick},
};

constexpr ActionChoice kGraphicsTabletOptions[] = {
    {IDS_SETTINGS_RIGHT_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kRightClick},
    {IDS_SETTINGS_MIDDLE_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kMiddleClick},
    {IDS_SETTINGS_LEFT_CLICK_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kLeftClick},
    {IDS_SETTINGS_UNDO_OPTION_LABEL, ::ash::mojom::StaticShortcutAction::kUndo},
    {IDS_SETTINGS_REDO_OPTION_LABEL, ::ash::mojom::StaticShortcutAction::kRedo},
    {IDS_SETTINGS_PREVIOUS_PAGE_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kPreviousPage},
    {IDS_SETTINGS_NEXT_PAGE_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kNextPage},
    {IDS_SETTINGS_ZOOM_IN_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kZoomIn},
    {IDS_SETTINGS_ZOOM_OUT_OPTION_LABEL,
     ::ash::mojom::StaticShortcutAction::kZoomOut},
    {IDS_SETTINGS_SCREENSHOT_OPTION_LABEL,
     AcceleratorAction::kTakePartialScreenshot},
    {IDS_SETTINGS_OVERVIEW_OPTION_LABEL, AcceleratorAction::kToggleOverview},
};

mojom::ActionTypePtr GetActionType(AcceleratorAction accelerator_action) {
  return mojom::ActionType::NewAcceleratorAction(accelerator_action);
}

mojom::ActionTypePtr GetActionType(
    ::ash::mojom::StaticShortcutAction static_shortcut_action) {
  return mojom::ActionType::NewStaticShortcutAction(static_shortcut_action);
}

mojom::ActionTypePtr GetActionTypeFromVariant(ActionTypeVariant variant) {
  return absl::visit([](auto&& value) { return GetActionType(value); },
                     variant);
}

template <typename T>
struct CustomDeviceKeyComparator {
  bool operator()(const T& device1, const T& device2) const {
    return device1->device_key < device2->device_key;
  }
};

template <typename T>
bool CompareDevices(const T& device1, const T& device2) {
  // Guarantees that external devices appear first in the
  // list.
  if (device1->is_external != device2->is_external) {
    return device1->is_external;
  }

  // Otherwise sort by most recently connected device (aka
  // id in descending order).
  return device1->id > device2->id;
}

template <>
bool CompareDevices(const ::ash::mojom::GraphicsTabletPtr& device1,
                    const ::ash::mojom::GraphicsTabletPtr& device2) {
  // Sort by most recently connected device (aka id in descending order).
  return device1->id > device2->id;
}

template <typename T>
std::vector<T> SanitizeAndSortDeviceList(std::vector<T> devices) {
  // Remove devices with duplicate `device_key`.
  base::flat_set<T, CustomDeviceKeyComparator<T>> devices_no_duplicates_set(
      std::move(devices));
  std::vector<T> devices_no_duplicates =
      std::move(devices_no_duplicates_set).extract();
  base::ranges::sort(devices_no_duplicates, CompareDevices<T>);
  return devices_no_duplicates;
}

void RecordKeyboardAmbientLightSensorDisabledCause(
    const power_manager::AmbientLightSensorChange_Cause& cause) {
  KeyboardAmbientLightSensorDisabledCause disabled_cause;
  switch (cause) {
    case power_manager::
        AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP:
      disabled_cause =
          KeyboardAmbientLightSensorDisabledCause::kUserRequestSettingsApp;
      break;
    case power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST:
      disabled_cause =
          KeyboardAmbientLightSensorDisabledCause::kBrightnessUserRequest;
      break;
    case power_manager::
        AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST_SETTINGS_APP:
      disabled_cause = KeyboardAmbientLightSensorDisabledCause::
          kBrightnessUserRequestSettingsApp;
      break;
    default:
      return;  // Exit function if none of the specified cases match
  }
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Keyboard.UserInitiated."
      "AmbientLightSensorDisabledCause",
      disabled_cause);
}

}  // namespace

InputDeviceSettingsProvider::InputDeviceSettingsProvider() {
  if (Shell::HasInstance()) {
    shell_observation_.Observe(ash::Shell::Get());
  }

  if (Shell::HasInstance() &&
      Shell::Get()->keyboard_brightness_control_delegate()) {
    keyboard_brightness_control_delegate_ =
        Shell::Get()->keyboard_brightness_control_delegate();
  } else {
    LOG(WARNING) << "InputDeviceSettingsProvider: Shell not available, did not "
                    "save KeyboardBrightnessControlDelegate.";
  }

  auto* controller = InputDeviceSettingsController::Get();
  if (!controller) {
    return;
  }

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    controller->AddObserver(this);
  }

  if (features::IsKeyboardBacklightControlInSettingsEnabled()) {
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    if (power_manager_client) {
      // power_manager_client may be NULL in unittests.
      power_manager_client->AddObserver(this);
      power_manager_client->GetSwitchStates(
          base::BindOnce(&InputDeviceSettingsProvider::OnReceiveSwitchStates,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

InputDeviceSettingsProvider::~InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();

  if (features::IsPeripheralCustomizationEnabled() && controller) {
    controller->StopObservingButtons();
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }

  if (features::IsInputDeviceSettingsSplitEnabled() && controller) {
    controller->RemoveObserver(this);
  }

  if (features::IsKeyboardBacklightControlInSettingsEnabled()) {
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    if (power_manager_client) {
      // power_manager_client may be NULL in unittests.
      power_manager_client->RemoveObserver(this);
    }
  }
}

void InputDeviceSettingsProvider::Initialize(content::WebUI* web_ui) {
  if (features::IsPeripheralCustomizationEnabled() && !widget_) {
    widget_ = views::Widget::GetWidgetForNativeWindow(
        web_ui->GetWebContents()->GetTopLevelNativeWindow());
    if (widget_) {
      widget_->AddObserver(this);
      HandleObserving();
    }
  }
}

void InputDeviceSettingsProvider::HandleObserving() {
  if (!widget_) {
    return;
  }

  bool previous = observing_paused_;
  const bool widget_open = !widget_->IsClosed();
  const bool widget_active = widget_->IsActive();
  const bool widget_visible = widget_->IsVisible();
  observing_paused_ = !(widget_open && widget_visible && widget_active);

  if (observing_paused_ == previous) {
    return;
  }

  if (observing_paused_) {
    InputDeviceSettingsController::Get()->StopObservingButtons();
    return;
  }

  for (const auto& id : observing_devices_) {
    InputDeviceSettingsController::Get()->StartObservingButtons(id);
  }
}

void InputDeviceSettingsProvider::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  if (keyboard_brightness_observer_.is_bound()) {
    keyboard_brightness_observer_->OnKeyboardBrightnessChanged(
        change.percent());
  }
}

void InputDeviceSettingsProvider::KeyboardAmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& change) {
  if (keyboard_ambient_light_sensor_observer_.is_bound()) {
    keyboard_ambient_light_sensor_observer_
        ->OnKeyboardAmbientLightSensorEnabledChanged(change.sensor_enabled());
  }

  if (features::IsKeyboardBacklightControlInSettingsEnabled() &&
      !change.sensor_enabled()) {
    RecordKeyboardAmbientLightSensorDisabledCause(change.cause());
  }
}

void InputDeviceSettingsProvider::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  HandleObserving();
}

void InputDeviceSettingsProvider::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  HandleObserving();
}

void InputDeviceSettingsProvider::OnWidgetDestroyed(views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = nullptr;
  // Reset observing paused since context was lost on the current state of the
  // settings app window.
  observing_paused_ = true;
  InputDeviceSettingsController::Get()->StopObservingButtons();
}

void InputDeviceSettingsProvider::OnShellDestroying() {
  // Set `KeyboardBrightnessControlDelegate` to null when shell destroys.
  keyboard_brightness_control_delegate_ = nullptr;
  shell_observation_.Reset();
}

void InputDeviceSettingsProvider::StartObserving(uint32_t device_id) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  observing_devices_.insert(device_id);
  if (!observing_paused_) {
    InputDeviceSettingsController::Get()->StartObservingButtons(device_id);
  }
}

void InputDeviceSettingsProvider::StopObserving() {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  observing_devices_.clear();
  InputDeviceSettingsController::Get()->StopObservingButtons();
}

void InputDeviceSettingsProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void InputDeviceSettingsProvider::RestoreDefaultKeyboardRemappings(
    uint32_t device_id) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->RestoreDefaultKeyboardRemappings(
      device_id);
}

void InputDeviceSettingsProvider::SetKeyboardSettings(
    uint32_t device_id,
    ::ash::mojom::KeyboardSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  if (!InputDeviceSettingsController::Get()->SetKeyboardSettings(
          device_id, std::move(settings))) {
    NotifyKeyboardsUpdated();
  }
}

void InputDeviceSettingsProvider::SetPointingStickSettings(
    uint32_t device_id,
    ::ash::mojom::PointingStickSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  if (!InputDeviceSettingsController::Get()->SetPointingStickSettings(
          device_id, std::move(settings))) {
    NotifyPointingSticksUpdated();
  }
}

void InputDeviceSettingsProvider::SetMouseSettings(
    uint32_t device_id,
    ::ash::mojom::MouseSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  if (!InputDeviceSettingsController::Get()->SetMouseSettings(
          device_id, std::move(settings))) {
    NotifyMiceUpdated();
  }
}

void InputDeviceSettingsProvider::SetTouchpadSettings(
    uint32_t device_id,
    ::ash::mojom::TouchpadSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  if (!InputDeviceSettingsController::Get()->SetTouchpadSettings(
          device_id, std::move(settings))) {
    NotifyTouchpadsUpdated();
  }
}

void InputDeviceSettingsProvider::SetGraphicsTabletSettings(
    uint32_t device_id,
    ::ash::mojom::GraphicsTabletSettingsPtr settings) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  if (!InputDeviceSettingsController::Get()->SetGraphicsTabletSettings(
          device_id, std::move(settings))) {
    NotifyGraphicsTabletUpdated();
  }
}

void InputDeviceSettingsProvider::SetKeyboardBrightness(double percent) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  if (!keyboard_brightness_control_delegate_) {
    LOG(ERROR) << "InputDeviceSettingsProvider: BrightnessControlDelegate not "
                  "available when setting keyboard brightness.";
    return;
  }
  keyboard_brightness_control_delegate_->HandleSetKeyboardBrightness(
      percent, /*gradual=*/true, KeyboardBrightnessChangeSource::kSettingsApp);
}

void InputDeviceSettingsProvider::SetKeyboardAmbientLightSensorEnabled(
    bool enabled) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  if (!keyboard_brightness_control_delegate_) {
    LOG(ERROR) << "InputDeviceSettingsProvider: BrightnessControlDelegate not "
                  "available when setting keyboard ambient light sensor.";
    return;
  }
  keyboard_brightness_control_delegate_
      ->HandleSetKeyboardAmbientLightSensorEnabled(
          enabled, KeyboardAmbientLightSensorEnabledChangeSource::kSettingsApp);

  // Record the keyboard auto-brightness toggle event.
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed",
      /*sample=*/enabled);
}

void InputDeviceSettingsProvider::OnReceiveKeyboardBrightness(
    std::optional<double> brightness_percent) {
  keyboard_brightness_observer_->OnKeyboardBrightnessChanged(
      brightness_percent.value_or(kDefaultKeyboardBrightness));
}

void InputDeviceSettingsProvider::OnReceiveKeyboardAmbientLightSensorEnabled(
    std::optional<bool> keyboard_ambient_light_sensor_enabled) {
  keyboard_ambient_light_sensor_observer_
      ->OnKeyboardAmbientLightSensorEnabledChanged(
          keyboard_ambient_light_sensor_enabled.value_or(true));
}

void InputDeviceSettingsProvider::ObserveKeyboardSettings(
    mojo::PendingRemote<mojom::KeyboardSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = keyboard_settings_observers_.Add(std::move(observer));
  auto* keyboard_settings_observer = keyboard_settings_observers_.Get(id);
  keyboard_settings_observer->OnKeyboardListUpdated(SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards()));
  keyboard_settings_observer->OnKeyboardPoliciesUpdated(
      InputDeviceSettingsController::Get()->GetKeyboardPolicies().Clone());
}

void InputDeviceSettingsProvider::ObserveTouchpadSettings(
    mojo::PendingRemote<mojom::TouchpadSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = touchpad_settings_observers_.Add(std::move(observer));
  touchpad_settings_observers_.Get(id)->OnTouchpadListUpdated(
      SanitizeAndSortDeviceList(
          InputDeviceSettingsController::Get()->GetConnectedTouchpads()));
}

void InputDeviceSettingsProvider::ObservePointingStickSettings(
    mojo::PendingRemote<mojom::PointingStickSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = pointing_stick_settings_observers_.Add(std::move(observer));
  pointing_stick_settings_observers_.Get(id)->OnPointingStickListUpdated(
      SanitizeAndSortDeviceList(
          InputDeviceSettingsController::Get()->GetConnectedPointingSticks()));
}

void InputDeviceSettingsProvider::ObserveMouseSettings(
    mojo::PendingRemote<mojom::MouseSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = mouse_settings_observers_.Add(std::move(observer));
  auto* mouse_settings_observer = mouse_settings_observers_.Get(id);
  mouse_settings_observer->OnMouseListUpdated(SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedMice()));
  mouse_settings_observer->OnMousePoliciesUpdated(
      InputDeviceSettingsController::Get()->GetMousePolicies().Clone());
}

void InputDeviceSettingsProvider::ObserveGraphicsTabletSettings(
    mojo::PendingRemote<mojom::GraphicsTabletSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = graphics_tablet_settings_observers_.Add(std::move(observer));
  auto* graphics_tablet_settings_observer =
      graphics_tablet_settings_observers_.Get(id);
  graphics_tablet_settings_observer->OnGraphicsTabletListUpdated(
      SanitizeAndSortDeviceList(
          InputDeviceSettingsController::Get()->GetConnectedGraphicsTablets()));
}

void InputDeviceSettingsProvider::ObserveButtonPresses(
    mojo::PendingRemote<mojom::ButtonPressObserver> observer) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  button_press_observers_.Add(std::move(observer));
}

void InputDeviceSettingsProvider::ObserveKeyboardBrightness(
    mojo::PendingRemote<mojom::KeyboardBrightnessObserver> observer) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  keyboard_brightness_observer_.reset();
  keyboard_brightness_observer_.Bind(std::move(observer));

  // Get the initial keyboard brightness when first register the observer.
  keyboard_brightness_control_delegate_->HandleGetKeyboardBrightness(
      base::BindOnce(&InputDeviceSettingsProvider::OnReceiveKeyboardBrightness,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceSettingsProvider::ObserveKeyboardAmbientLightSensor(
    mojo::PendingRemote<mojom::KeyboardAmbientLightSensorObserver> observer) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  keyboard_ambient_light_sensor_observer_.reset();
  keyboard_ambient_light_sensor_observer_.Bind(std::move(observer));

  // Get the initial keyboard ambient light sensor enabled status when first
  // register the observer.
  keyboard_brightness_control_delegate_
      ->HandleGetKeyboardAmbientLightSensorEnabled(
          base::BindOnce(&InputDeviceSettingsProvider::
                             OnReceiveKeyboardAmbientLightSensorEnabled,
                         weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceSettingsProvider::ObserveLidState(
    mojo::PendingRemote<mojom::LidStateObserver> observer,
    ObserveLidStateCallback callback) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  lid_state_observers_.Add(std::move(observer));
  std::move(callback).Run(is_lid_open_);
}

void InputDeviceSettingsProvider::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks time) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  // If the lid state is open or if the lid state sensors is not present, the
  // lid is considered open
  is_lid_open_ = state != chromeos::PowerManagerClient::LidState::CLOSED;
  for (auto& observer : lid_state_observers_) {
    observer->OnLidStateChanged(is_lid_open_);
  }
}

void InputDeviceSettingsProvider::OnReceiveSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  if (switch_states.has_value()) {
    LidEventReceived(switch_states->lid_state, /*time=*/{});
  }
}

void InputDeviceSettingsProvider::OnCustomizableMouseButtonPressed(
    const ::ash::mojom::Mouse& mouse,
    const ::ash::mojom::Button& button) {
  if (observing_paused_) {
    return;
  }

  for (const auto& observer : button_press_observers_) {
    observer->OnButtonPressed(button.Clone());
  }
}

void InputDeviceSettingsProvider::OnCustomizablePenButtonPressed(
    const ::ash::mojom::GraphicsTablet& graphics_tablet,
    const ::ash::mojom::Button& button) {
  if (observing_paused_) {
    return;
  }

  for (const auto& observer : button_press_observers_) {
    observer->OnButtonPressed(button.Clone());
  }
}

void InputDeviceSettingsProvider::OnCustomizableTabletButtonPressed(
    const ::ash::mojom::GraphicsTablet& graphics_tablet,
    const ::ash::mojom::Button& button) {
  if (observing_paused_) {
    return;
  }

  for (const auto& observer : button_press_observers_) {
    observer->OnButtonPressed(button.Clone());
  }
}

void InputDeviceSettingsProvider::OnKeyboardBatteryInfoChanged(
    const ::ash::mojom::Keyboard& keyboard) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnGraphicsTabletBatteryInfoChanged(
    const ::ash::mojom::GraphicsTablet& graphics_tablet) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyGraphicsTabletUpdated();
}

void InputDeviceSettingsProvider::OnMouseBatteryInfoChanged(
    const ::ash::mojom::Mouse& mouse) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadBatteryInfoChanged(
    const ::ash::mojom::Touchpad& touchpad) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnMouseCompanionAppInfoChanged(
    const ::ash::mojom::Mouse& mouse) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardCompanionAppInfoChanged(
    const ::ash::mojom::Keyboard& keyboard) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadCompanionAppInfoChanged(
    const ::ash::mojom::Touchpad& touchpad) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnGraphicsTabletCompanionAppInfoChanged(
    const ::ash::mojom::GraphicsTablet& graphics_tablet) {
  CHECK(features::IsWelcomeExperienceEnabled());
  NotifyGraphicsTabletUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardConnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardDisconnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardSettingsUpdated(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardPoliciesUpdated(
    const ::ash::mojom::KeyboardPolicies& keyboard_policies) {
  for (const auto& observer : keyboard_settings_observers_) {
    observer->OnKeyboardPoliciesUpdated(keyboard_policies.Clone());
  }
}

void InputDeviceSettingsProvider::OnTouchpadConnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadDisconnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadSettingsUpdated(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickConnected(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickDisconnected(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickSettingsUpdated(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnMouseConnected(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnMouseDisconnected(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnMouseSettingsUpdated(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnGraphicsTabletConnected(
    const ::ash::mojom::GraphicsTablet& graphics_tablet) {
  NotifyGraphicsTabletUpdated();
}

void InputDeviceSettingsProvider::OnGraphicsTabletDisconnected(
    const ::ash::mojom::GraphicsTablet& graphics_tablet) {
  NotifyGraphicsTabletUpdated();
}

void InputDeviceSettingsProvider::OnGraphicsTabletSettingsUpdated(
    const ::ash::mojom::GraphicsTablet& graphics_tablet) {
  NotifyGraphicsTabletUpdated();
}

void InputDeviceSettingsProvider::OnMousePoliciesUpdated(
    const ::ash::mojom::MousePolicies& mouse) {
  for (const auto& observer : mouse_settings_observers_) {
    observer->OnMousePoliciesUpdated(
        InputDeviceSettingsController::Get()->GetMousePolicies().Clone());
  }
}

void InputDeviceSettingsProvider::NotifyKeyboardsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto keyboards = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards());
  for (const auto& observer : keyboard_settings_observers_) {
    observer->OnKeyboardListUpdated(mojo::Clone(keyboards));
  }
}

void InputDeviceSettingsProvider::NotifyTouchpadsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto touchpads = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedTouchpads());
  for (const auto& observer : touchpad_settings_observers_) {
    observer->OnTouchpadListUpdated(mojo::Clone(touchpads));
  }
}

void InputDeviceSettingsProvider::NotifyPointingSticksUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto pointing_sticks = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedPointingSticks());
  for (const auto& observer : pointing_stick_settings_observers_) {
    observer->OnPointingStickListUpdated(mojo::Clone(pointing_sticks));
  }
}

void InputDeviceSettingsProvider::NotifyMiceUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto mice = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedMice());
  for (const auto& observer : mouse_settings_observers_) {
    observer->OnMouseListUpdated(mojo::Clone(mice));
  }
}

void InputDeviceSettingsProvider::NotifyGraphicsTabletUpdated() {
  CHECK(features::IsPeripheralCustomizationEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  auto graphics_tablets = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedGraphicsTablets());
  for (const auto& observer : graphics_tablet_settings_observers_) {
    observer->OnGraphicsTabletListUpdated(mojo::Clone(graphics_tablets));
  }
}

void InputDeviceSettingsProvider::SetWidgetForTesting(views::Widget* widget) {
  widget_ = widget;
  widget_->AddObserver(this);
  HandleObserving();
}

void InputDeviceSettingsProvider::OnReceiveDeviceImage(
    GetDeviceIconImageCallback callback,
    const std::optional<std::string>& data_url) {
  std::move(callback).Run(data_url);
}

void InputDeviceSettingsProvider::GetDeviceIconImage(
    const std::string& device_key,
    GetDeviceIconImageCallback callback) {
  CHECK(features::IsWelcomeExperienceEnabled());
  CHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->GetDeviceImageDataUrl(
      device_key,
      base::BindOnce(&InputDeviceSettingsProvider::OnReceiveDeviceImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDeviceSettingsProvider::LaunchCompanionApp(
    const std::string& package_id_str) {
  CHECK(features::IsWelcomeExperienceEnabled());
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto package_id = apps::PackageId::FromString(package_id_str);
  CHECK(package_id.has_value());
  auto app_id = apps_util::GetAppWithPackageId(&*profile, package_id.value());
  CHECK(app_id.has_value());
  apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->LaunchAppWithParams(apps::AppLaunchParams(
          app_id.value(), apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW,
          apps::LaunchSource::kFromManagementApi));
}

void InputDeviceSettingsProvider::
    GetActionsForGraphicsTabletButtonCustomization(
        GetActionsForGraphicsTabletButtonCustomizationCallback callback) {
  std::vector<mojom::ActionChoicePtr> choices;
  for (const auto& choice : kGraphicsTabletOptions) {
    choices.push_back(mojom::ActionChoice::New(
        GetActionTypeFromVariant(choice.action_variant),
        l10n_util::GetStringUTF8(choice.id)));
  }
  std::move(callback).Run(std::move(choices));
}

void InputDeviceSettingsProvider::GetActionsForMouseButtonCustomization(
    GetActionsForMouseButtonCustomizationCallback callback) {
  std::vector<mojom::ActionChoicePtr> choices;
  for (const auto& choice : kMouseButtonOptions) {
    choices.push_back(mojom::ActionChoice::New(
        GetActionTypeFromVariant(choice.action_variant),
        l10n_util::GetStringUTF8(choice.id)));
  }
  std::move(callback).Run(std::move(choices));
}

void InputDeviceSettingsProvider::GetMetaKeyToDisplay(
    GetMetaKeyToDisplayCallback callback) {
  std::move(callback).Run(
      Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay());
}

void InputDeviceSettingsProvider::OnReceiveHasKeyboardBacklight(
    HasKeyboardBacklightCallback callback,
    std::optional<bool> has_keyboard_backlight) {
  std::move(callback).Run(has_keyboard_backlight.value_or(false));
}

void InputDeviceSettingsProvider::OnReceiveHasAmbientLightSensor(
    HasAmbientLightSensorCallback callback,
    std::optional<bool> has_ambient_light_sensor) {
  std::move(callback).Run(has_ambient_light_sensor.value_or(false));
}

void InputDeviceSettingsProvider::HasKeyboardBacklight(
    HasKeyboardBacklightCallback callback) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  chromeos::PowerManagerClient::Get()->HasKeyboardBacklight(base::BindOnce(
      &InputDeviceSettingsProvider::OnReceiveHasKeyboardBacklight,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDeviceSettingsProvider::HasAmbientLightSensor(
    HasAmbientLightSensorCallback callback) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  chromeos::PowerManagerClient::Get()->HasAmbientLightSensor(base::BindOnce(
      &InputDeviceSettingsProvider::OnReceiveHasAmbientLightSensor,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDeviceSettingsProvider::IsRgbKeyboardSupported(
    IsRgbKeyboardSupportedCallback callback) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  std::move(callback).Run(
      Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported());
}

void InputDeviceSettingsProvider::RecordKeyboardColorLinkClicked() {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Keyboard.ColorLinkClicked", true);
}

void InputDeviceSettingsProvider::RecordKeyboardBrightnessChangeFromSlider(
    double percent) {
  DCHECK(features::IsKeyboardBacklightControlInSettingsEnabled());
  DCHECK(0 <= percent && percent <= 100);
  base::UmaHistogramPercentage(
      "ChromeOS.Settings.Device.Keyboard.BrightnessSliderAdjusted", percent);
}

}  // namespace ash::settings
