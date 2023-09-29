// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.mojom-forward.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash::settings {

namespace {

using ActionTypeVariant =
    absl::variant<AcceleratorAction, ::ash::mojom::StaticShortcutAction>;

// Used to represent a constant version of the mojom::ActionChoice struct.
struct ActionChoice {
  const char* name;
  ActionTypeVariant action_variant;
};

// TODO(dpad): Update list to official list of actions.
// TODO(b/286930911): Translate action string names.
constexpr ActionChoice kMouseButtonOptions[] = {
    {"Disable", ::ash::mojom::StaticShortcutAction::kDisable},
    {"Volume mute", AcceleratorAction::kVolumeMuteToggle},
    {"Microphone mute", AcceleratorAction::kMicrophoneMuteToggle},
    {"Play/Pause media", AcceleratorAction::kMediaPlayPause},
    {"Overview", AcceleratorAction::kToggleOverview},
    {"Screenshot", AcceleratorAction::kTakeScreenshot},
    {"Emoji Picker", AcceleratorAction::kShowEmojiPicker},
    {"Turn on high contrast", AcceleratorAction::kToggleHighContrast},
    {"Turn on magnifier", AcceleratorAction::kToggleFullscreenMagnifier},
    {"Turn on dictation", AcceleratorAction::kEnableOrToggleDictation},
    {"Copy", ::ash::mojom::StaticShortcutAction::kCopy},
    {"Paste", ::ash::mojom::StaticShortcutAction::kPaste},
};

// TODO(dpad): Update list to official list of actions.
// TODO(b/286930911): Translate action string names.
constexpr ActionChoice kGraphicsTabletOptions[] = {
    {"Disable", ::ash::mojom::StaticShortcutAction::kDisable},
    {"Volume mute", AcceleratorAction::kVolumeMuteToggle},
    {"Microphone mute", AcceleratorAction::kMicrophoneMuteToggle},
    {"Play/Pause media", AcceleratorAction::kMediaPlayPause},
    {"Overview", AcceleratorAction::kToggleOverview},
    {"Screenshot", AcceleratorAction::kTakeScreenshot},
    {"Emoji Picker", AcceleratorAction::kShowEmojiPicker},
    {"Turn on high contrast", AcceleratorAction::kToggleHighContrast},
    {"Turn on magnifier", AcceleratorAction::kToggleFullscreenMagnifier},
    {"Turn on dictation", AcceleratorAction::kEnableOrToggleDictation},
    {"Copy", ::ash::mojom::StaticShortcutAction::kCopy},
    {"Paste", ::ash::mojom::StaticShortcutAction::kPaste},
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

}  // namespace

InputDeviceSettingsProvider::InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (!controller) {
    return;
  }

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    controller->AddObserver(this);
  }
}

InputDeviceSettingsProvider::~InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (!controller) {
    return;
  }

  if (features::IsPeripheralCustomizationEnabled()) {
    controller->StopObservingButtons();
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    controller->RemoveObserver(this);
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
  InputDeviceSettingsController::Get()->SetKeyboardSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::SetPointingStickSettings(
    uint32_t device_id,
    ::ash::mojom::PointingStickSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetPointingStickSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::SetMouseSettings(
    uint32_t device_id,
    ::ash::mojom::MouseSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetMouseSettings(device_id,
                                                         std::move(settings));
}

void InputDeviceSettingsProvider::SetTouchpadSettings(
    uint32_t device_id,
    ::ash::mojom::TouchpadSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetTouchpadSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::SetGraphicsTabletSettings(
    uint32_t device_id,
    ::ash::mojom::GraphicsTabletSettingsPtr settings) {
  DCHECK(features::IsPeripheralCustomizationEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetGraphicsTabletSettings(
      device_id, std::move(settings));
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

void InputDeviceSettingsProvider::
    GetActionsForGraphicsTabletButtonCustomization(
        GetActionsForGraphicsTabletButtonCustomizationCallback callback) {
  std::vector<mojom::ActionChoicePtr> choices;
  for (const auto& choice : kGraphicsTabletOptions) {
    choices.push_back(mojom::ActionChoice::New(
        GetActionTypeFromVariant(choice.action_variant), choice.name));
  }
  std::move(callback).Run(std::move(choices));
}

void InputDeviceSettingsProvider::GetActionsForMouseButtonCustomization(
    GetActionsForMouseButtonCustomizationCallback callback) {
  std::vector<mojom::ActionChoicePtr> choices;
  for (const auto& choice : kMouseButtonOptions) {
    choices.push_back(mojom::ActionChoice::New(
        GetActionTypeFromVariant(choice.action_variant), choice.name));
  }
  std::move(callback).Run(std::move(choices));
}

}  // namespace ash::settings
