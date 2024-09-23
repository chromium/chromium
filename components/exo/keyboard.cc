// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/keyboard.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/input_trace.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_modifiers.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/xkb_tracker.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/events.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// This value must be bigger than the priority for DataDevice.
constexpr int kKeyboardSeatObserverPriority = 1;
static_assert(Seat::IsValidObserverPriority(kKeyboardSeatObserverPriority),
              "kKeyboardSeatObserverPriority is not in the valid range.");

// Delay until a key state change expected to be acknowledged is expired.
constexpr int kExpirationDelayForPendingKeyAcksMs = 1000;

// The accelerator keys reserved to be processed by chrome.
constexpr struct {
  ui::KeyboardCode keycode;
  int modifiers;
} kReservedAccelerators[] = {
    {ui::VKEY_F13, ui::EF_NONE},
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
    {ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}};

bool ProcessAccelerator(Surface* surface, const ui::KeyEvent* event) {
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    return focus_manager->ProcessAccelerator(ui::Accelerator(*event));
  }
  return false;
}

bool IsVirtualKeyboardEnabled() {
  return keyboard::GetAccessibilityKeyboardEnabled() ||
         keyboard::GetTouchKeyboardEnabled() ||
         (keyboard::KeyboardUIController::HasInstance() &&
          keyboard::KeyboardUIController::Get()->IsEnableFlagSet(
              keyboard::KeyboardEnableFlag::kCommandLineEnabled));
}

bool IsReservedAccelerator(const ui::KeyEvent* event) {
  for (const auto& accelerator : kReservedAccelerators) {
    if (event->flags() == accelerator.modifiers &&
        event->key_code() == accelerator.keycode) {
      return true;
    }
  }
  return false;
}

// Returns false if an accelerator is not reserved or it's not enabled.
bool ProcessAcceleratorIfReserved(Surface* surface, ui::KeyEvent* event) {
  return IsReservedAccelerator(event) && ProcessAccelerator(surface, event);
}

// Returns true if the surface needs to support IME.
// TODO(yhanada, https://crbug.com/847500): Remove this when we find a way
// to fix https://crbug.com/847500 without breaking ARC apps/Lacros browser.
bool IsImeSupportedSurface(Surface* surface) {
  aura::Window* window = surface->window();
  while (window) {
    const auto app_type = window->GetProperty(chromeos::kAppTypeKey);
    switch (app_type) {
      case chromeos::AppType::ARC_APP:
      case chromeos::AppType::CROSTINI_APP:
      case chromeos::AppType::LACROS:
        return true;
      default:
        // Do nothing.
        break;
    }
    // For notifications, billing surfaces, etc. AppType::ARC_APP is not set
    // despite them being from ARC. Ideally AppType should be added to them, but
    // there is a risk that breaks other features e.g. full restore.
    // TODO(tetsui): find a way to remove this.
    if (window->GetProperty(aura::client::kSkipImeProcessing))
      return true;

    if (aura::Window* transient_parent = wm::GetTransientParent(window)) {
      window = transient_parent;
    } else {
      window = window->parent();
    }
  }
  return false;
}

// Returns true if the surface can consume ash accelerators.
bool CanConsumeAshAccelerators(Surface* surface) {
  aura::Window* window = surface->window();
  for (; window; window = window->parent()) {
    const auto app_type = window->GetProperty(chromeos::kAppTypeKey);
    // TOOD(hidehiko): get rid of this if check, after introducing capability,
    // followed by ARC/Crostini migration.
    if (app_type == chromeos::AppType::LACROS) {
      return surface->is_keyboard_shortcuts_inhibited();
    }
  }
  return true;
}

// Returns true if an accelerator is an ash accelerator which can be handled
// before sending it to client and it is actually processed by ash-chrome.
bool ProcessAshAcceleratorIfPossible(Surface* surface, ui::KeyEvent* event) {
  // Process ash accelerators before sending it to client only when the client
  // should not consume ash accelerators. (e.g. Lacros-chrome)
  if (CanConsumeAshAccelerators(surface))
    return false;

  // If accelerators can be processed by browser, send it to the app.
  static const base::NoDestructor<std::vector<ui::Accelerator>>
      kAppHandlingAccelerators([] {
        std::vector<ui::Accelerator> result;
        for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
          const auto& ash_entry = ash::kAcceleratorData[i];
          if (base::Contains(base::span<const ash::AcceleratorAction>(
                                 ash::kActionsInterceptableByBrowser,
                                 ash::kActionsInterceptableByBrowserLength),
                             ash_entry.action) ||
              base::Contains(base::span<const ash::AcceleratorAction>(
                                 ash::kActionsDuplicatedWithBrowser,
                                 ash::kActionsDuplicatedWithBrowserLength),
                             ash_entry.action)) {
            result.emplace_back(ash_entry.keycode, ash_entry.modifiers);
          }
        }
        return result;
      }());
  ui::Accelerator accelerator(*event);
  if (base::Contains(*kAppHandlingAccelerators, accelerator))
    return false;

  return ash::AcceleratorController::Get()->Process(accelerator);
}

bool IsAutoRepeatEnabled(const ui::KeyEvent& event) {
  const auto* properties = event.properties();
  if (!properties) {
    return true;
  }
  return !ui::HasKeyEventSuppressAutoRepeat(*properties);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Keyboard, public:

Keyboard::Keyboard(std::unique_ptr<KeyboardDelegate> delegate, Seat* seat)
    : delegate_(std::move(delegate)),
      seat_(seat),
      expiration_delay_for_pending_key_acks_(
          base::Milliseconds(kExpirationDelayForPendingKeyAcksMs)) {
  seat_->AddObserver(this, kKeyboardSeatObserverPriority);
  auto* keyboard_controller = ash::KeyboardController::Get();
  keyboard_controller->AddObserver(this);
  ash::ImeControllerImpl* ime_controller = ash::Shell::Get()->ime_controller();
  ime_controller->AddObserver(this);

  delegate_->OnKeyboardLayoutUpdated(seat_->xkb_tracker()->GetKeymap().get());
  OnSurfaceFocused(seat_->GetFocusedSurface(), nullptr,
                   !!seat_->GetFocusedSurface());

  // Send the initial key repeat settings, iff it is already initialized.
  // If not, that means Profile is not yet initialized, thus skipping,
  // because when it is initialized, OnKeyRepeatSettingsChanged is called
  // by KeyboardController.
  auto key_repeat_settings = keyboard_controller->GetKeyRepeatSettings();
  if (key_repeat_settings.has_value())
    OnKeyRepeatSettingsChanged(key_repeat_settings.value());
}

Keyboard::~Keyboard() {
  RemoveEventHandler();
  for (KeyboardObserver& observer : observer_list_)
    observer.OnKeyboardDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);

  ash::Shell::Get()->ime_controller()->RemoveObserver(this);
  ash::KeyboardController::Get()->RemoveObserver(this);
  seat_->RemoveObserver(this);
}

bool Keyboard::HasDeviceConfigurationDelegate() const {
  return !!device_configuration_delegate_;
}

void Keyboard::SetDeviceConfigurationDelegate(
    KeyboardDeviceConfigurationDelegate* delegate) {
  device_configuration_delegate_ = delegate;
  UpdateKeyboardType();
}

void Keyboard::AddObserver(KeyboardObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool Keyboard::HasObserver(KeyboardObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Keyboard::RemoveObserver(KeyboardObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Keyboard::SetNeedKeyboardKeyAcks(bool need_acks) {
  RemoveEventHandler();
  are_keyboard_key_acks_needed_ = need_acks;
  AddEventHandler();
}

bool Keyboard::AreKeyboardKeyAcksNeeded() const {
  // Keyboard class doesn't need key acks while the spoken feedback is enabled.
  // While the spoken feedback is enabled, a key event is sent to both of a
  // wayland client and Chrome to give a chance to work to Chrome OS's
  // shortcuts.
  return are_keyboard_key_acks_needed_;
}

void Keyboard::AckKeyboardKey(uint32_t serial, bool handled) {
  auto it = pending_key_acks_.find(serial);
  if (it == pending_key_acks_.end())
    return;

  auto* key_event = &it->second.first;
  if (!handled && !key_event->handled() && focus_)
    ProcessAccelerator(focus_, key_event);
  pending_key_acks_.erase(serial);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Keyboard::OnKeyEvent(ui::KeyEvent* event) {
  if (!focus_ || seat_->was_shutdown())
    return;

  DCHECK(GetShellRootSurface(static_cast<aura::Window*>(event->target())) ||
         Surface::AsSurface(static_cast<aura::Window*>(event->target())));

  // Ignore synthetic key repeat events.
  if (event->is_repeat()) {
    // Clients should not see key repeat events and instead handle them on the
    // client side.
    // Mark the key repeat events as handled to avoid them from invoking
    // accelerators.
    event->SetHandled();
    return;
  }

  TRACE_EXO_INPUT_EVENT(event);

  // Process reserved accelerators or ash accelerators which need to be handled
  // before sending it to client.
  if (ProcessAcceleratorIfReserved(focus_, event) ||
      ProcessAshAcceleratorIfPossible(focus_, event)) {
    // Discard a key press event if the corresponding accelerator is handled.
    event->SetHandled();
    // The current focus might have been reset while processing accelerators.
    if (!focus_)
      return;
  }

  // When IME ate a key event, we use the event only for tracking key states and
  // ignore for further processing. Otherwise it is handled in two places (IME
  // and client) and causes undesired behavior.
  // If the window should receive a key event before IME, Exo should send any
  // key events to a client. The client will send back the events to IME if
  // needed.
  const bool consumed_by_ime =
      !focus_->window()->GetProperty(aura::client::kSkipImeProcessing) &&
      ConsumedByIme(*event);

  // Currently, physical keycode is tracked in Seat, assuming that the
  // Keyboard::OnKeyEvent is called between Seat::WillProcessEvent and
  // Seat::DidProcessEvent. However, if IME is enabled, it is no longer true,
  // because IME work in async approach, and on its dispatching, call stack
  // is split so actually Keyboard::OnKeyEvent is called after
  // Seat::DidProcessEvent.
  // TODO(yhanada): This is a quick fix for https://crbug.com/859071. Remove
  // ARC-/Lacros-specific code path once we can find a way to manage
  // press/release events pair for synthetic events.
  PhysicalCode physical_code =
      seat_->physical_code_for_currently_processing_event();
  const auto* physical_dom_code = std::get_if<ui::DomCode>(&physical_code);
  if (physical_dom_code && *physical_dom_code == ui::DomCode::NONE &&
      focused_on_ime_supported_surface_) {
    // This key event is a synthetic event.
    // Consider DomCode field of the event as a physical code
    // for synthetic events when focus surface belongs to an ARC application.
    physical_code = event->code();
  }

  switch (event->type()) {
    case ui::EventType::kKeyPressed: {
      auto it = pressed_keys_.find(physical_code);
      const bool should_handle =
          (it == pressed_keys_.end()) ||
          (event->flags() & ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
      const bool is_physical_code_none =
          physical_dom_code && *physical_dom_code == ui::DomCode::NONE;
      if (should_handle && !event->handled() && !is_physical_code_none) {
        if (bool auto_repeat_enabled = IsAutoRepeatEnabled(*event);
            auto_repeat_enabled != auto_repeat_enabled_) {
          auto_repeat_enabled_ = auto_repeat_enabled;
          if (auto settings =
                  ash::KeyboardController::Get()->GetKeyRepeatSettings();
              settings.has_value()) {
            OnKeyRepeatSettingsChanged(*settings);
          }
        }

        for (auto& observer : observer_list_) {
          observer.OnKeyboardKey(event->time_stamp(), event->code(), true);
        }

        if (!consumed_by_ime) {
          // Process key press event if not already handled and not already
          // pressed.
          uint32_t serial = delegate_->OnKeyboardKey(event->time_stamp(),
                                                     event->code(), true);
          if (AreKeyboardKeyAcksNeeded()) {
            pending_key_acks_.insert(
                {serial,
                 {*event, base::TimeTicks::Now() +
                              expiration_delay_for_pending_key_acks_}});
            event->SetHandled();
          }
        }
        // Keep track of both the physical code and potentially re-written
        // code that this event generated.
        pressed_keys_[physical_code].emplace(event->code(), consumed_by_ime);
      } else if (!should_handle && !event->handled()) {
        // Non-repeate key events for already pressed key can be sent in some
        // cases (e.g. Holding 'A' key then holding 'B' key then releasing 'A'
        // key sends a non-repeat 'B' key press event).
        // When it happens, we don't want to send the press event to a client
        // and also want to avoid it from invoking any accelerator.
        if (AreKeyboardKeyAcksNeeded())
          event->SetHandled();
      }
    } break;
    case ui::EventType::kKeyReleased: {
      // Process key release event if currently pressed.
      auto key_state_set_iter = pressed_keys_.find(physical_code);
      if (key_state_set_iter == pressed_keys_.end()) {
        break;
      }

      auto& key_state_set = key_state_set_iter->second;
      auto key_state_iter = base::ranges::find(
          key_state_set, event->code(),
          [](const KeyState& key_state) { return key_state.code; });

      // If we can't find the specific key event to release, all previously
      // pressed events tied to this physical key should be released.
      auto [begin, end] =
          key_state_iter == key_state_set.end()
              ? std::pair(key_state_set.begin(), key_state_set.end())
              : std::pair(key_state_iter, key_state_iter + 1);
      for (auto iter = begin; iter != end; ++iter) {
        for (auto& observer : observer_list_) {
          observer.OnKeyboardKey(event->time_stamp(), iter->code, false);
        }

        if (!iter->consumed_by_ime) {
          // We use the code that was generated when the physical key was
          // pressed rather than the current event code. This allows events
          // to be re-written before dispatch, while still allowing the
          // client to track the state of the physical keyboard.
          uint32_t serial =
              delegate_->OnKeyboardKey(event->time_stamp(), iter->code, false);
          if (AreKeyboardKeyAcksNeeded()) {
            auto ack_it =
                pending_key_acks_
                    .insert(
                        {serial,
                         {*event, base::TimeTicks::Now() +
                                      expiration_delay_for_pending_key_acks_}})
                    .first;
            // Handled is not copied with Event's copy ctor, so explicitly copy
            // here.
            if (event->handled()) {
              ack_it->second.first.SetHandled();
            }
            event->SetHandled();
          }
        }
      }
      key_state_set.erase(begin, end);
      if (key_state_set.empty()) {
        pressed_keys_.erase(key_state_set_iter);
      }
    } break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (pending_key_acks_.empty())
    return;
  if (process_expired_pending_key_acks_pending_)
    return;

  ScheduleProcessExpiredPendingKeyAcks(expiration_delay_for_pending_key_acks_);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Keyboard::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  SetFocus(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// SeatObserver overrides:

void Keyboard::OnSurfaceFocused(Surface* gained_focus,
                                Surface* lost_focused,
                                bool has_focused_surface) {
  Surface* gained_focus_surface =
      gained_focus && delegate_->CanAcceptKeyboardEventsForSurface(gained_focus)
          ? gained_focus
          : nullptr;
  if (gained_focus_surface != focus_)
    SetFocus(gained_focus_surface);
}

void Keyboard::OnKeyboardModifierUpdated() {
  // XkbTracker must be updated in the Seat, before calling this method.
  if (focus_)
    delegate_->OnKeyboardModifiers(seat_->xkb_tracker()->GetModifiers());
}

////////////////////////////////////////////////////////////////////////////////
// ash::KeyboardControllerObserver overrides:

void Keyboard::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  UpdateKeyboardType();
}

void Keyboard::OnKeyRepeatSettingsChanged(
    const ash::KeyRepeatSettings& settings) {
  delegate_->OnKeyRepeatSettingsChanged(
      settings.enabled && auto_repeat_enabled_, settings.delay,
      settings.interval);
}

////////////////////////////////////////////////////////////////////////////////
// ash::ImeController::Observer overrides:

void Keyboard::OnCapsLockChanged(bool enabled) {}

void Keyboard::OnKeyboardLayoutNameChanged(const std::string& layout_name) {
  // XkbTracker must be updated in the Seat, before calling this method.
  // Ensured by the observer registration order.
  delegate_->OnKeyboardLayoutUpdated(seat_->xkb_tracker()->GetKeymap().get());
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard, private:

base::flat_map<PhysicalCode, base::flat_set<KeyState>>
Keyboard::GetPressedKeysForSurface(Surface* surface) {
  // Remove system keys from being sent as pressed keys unless the window
  // can consume them.
  base::flat_map<PhysicalCode, base::flat_set<KeyState>> filtered_keys =
      pressed_keys_;
  aura::Window* top_level = surface->window()->GetToplevelWindow();
  if (top_level && !ash::WindowState::Get(top_level)->CanConsumeSystemKeys()) {
    base::EraseIf(filtered_keys, [](auto& key_state_set_pair) {
      base::EraseIf(key_state_set_pair.second, [](auto& key_state) {
        return ash::AcceleratorController::IsSystemKey(key_state.key_code);
      });
      return key_state_set_pair.second.empty();
    });
  }
  return filtered_keys;
}

void Keyboard::SetFocus(Surface* surface) {
  if (focus_) {
    RemoveEventHandler();
    delegate_->OnKeyboardLeave(focus_);
    focus_->RemoveSurfaceObserver(this);
    focus_ = nullptr;
    pending_key_acks_.clear();
  }
  if (surface) {
    pressed_keys_ = seat_->pressed_keys();
    auto enter_keys = GetPressedKeysForSurface(surface);
    delegate_->OnKeyboardModifiers(seat_->xkb_tracker()->GetModifiers());
    delegate_->OnKeyboardEnter(surface, enter_keys);
    focus_ = surface;
    focus_->AddSurfaceObserver(this);
    focused_on_ime_supported_surface_ = IsImeSupportedSurface(surface);
    AddEventHandler();
  }
}

void Keyboard::ProcessExpiredPendingKeyAcks() {
  DCHECK(process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = false;

  // Check pending acks and process them as if it is handled if
  // expiration time passed.
  base::TimeTicks current_time = base::TimeTicks::Now();

  while (!pending_key_acks_.empty()) {
    auto it = pending_key_acks_.begin();
    const ui::KeyEvent event = it->second.first;

    if (it->second.second > current_time)
      break;

    // Expiration time has passed, assume the event was handled.
    pending_key_acks_.erase(it);
  }

  if (pending_key_acks_.empty())
    return;

  base::TimeDelta delay_until_next_process_expired_pending_key_acks =
      pending_key_acks_.begin()->second.second - current_time;
  ScheduleProcessExpiredPendingKeyAcks(
      delay_until_next_process_expired_pending_key_acks);
}

void Keyboard::ScheduleProcessExpiredPendingKeyAcks(base::TimeDelta delay) {
  DCHECK(!process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Keyboard::ProcessExpiredPendingKeyAcks,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Keyboard::AddEventHandler() {
  if (!focus_)
    return;

  // Toplevel window can be not ShellSurface, for example for a notification
  // surface.
  aura::Window* toplevel_window = focus_->window();
  if (toplevel_window->GetToplevelWindow())
    toplevel_window = toplevel_window->GetToplevelWindow();

  if (are_keyboard_key_acks_needed_)
    toplevel_window->AddPreTargetHandler(this);
  else
    toplevel_window->AddPostTargetHandler(this);
}

void Keyboard::RemoveEventHandler() {
  if (!focus_)
    return;

  // Toplevel window can be not ShellSurface, for example for a notification
  // surface.
  aura::Window* toplevel_window = focus_->window();
  if (toplevel_window->GetToplevelWindow())
    toplevel_window = toplevel_window->GetToplevelWindow();

  if (are_keyboard_key_acks_needed_)
    toplevel_window->RemovePreTargetHandler(this);
  else
    toplevel_window->RemovePostTargetHandler(this);
}

void Keyboard::UpdateKeyboardType() {
  if (!device_configuration_delegate_)
    return;

  // Ignore kAndroidDisabled which affects |enabled| and just test for a11y
  // and touch enabled keyboards. TODO(yhanada): Fix this using an Android
  // specific KeyboardUI implementation. https://crbug.com/897655.
  const bool is_physical = !IsVirtualKeyboardEnabled();
  device_configuration_delegate_->OnKeyboardTypeChanged(is_physical);
}

}  // namespace exo
