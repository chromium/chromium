// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/ime_controller_client_impl.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace {

using ::ash::input_method::InputMethodDescriptor;
using ::ash::input_method::InputMethodManager;
using ::ash::input_method::InputMethodUtil;
using ::ui::ime::InputMethodMenuManager;

ImeControllerClientImpl* g_ime_controller_client_instance = nullptr;

}  // namespace

ImeControllerClientImpl::ImeControllerClientImpl(InputMethodManager* manager)
    : input_method_manager_(manager) {
  DCHECK(input_method_manager_);
  input_method_manager_->AddObserver(this);
  input_method_manager_->AddImeMenuObserver(this);
  if (input_method_manager_->GetImeKeyboard())
    observation_.Observe(input_method_manager_->GetImeKeyboard());
  InputMethodMenuManager::GetInstance()->AddObserver(this);

  // This does not need to send the initial state to ash because that happens
  // via observers when the InputMethodManager initializes its list of IMEs.

  DCHECK(!g_ime_controller_client_instance);
  g_ime_controller_client_instance = this;
}

ImeControllerClientImpl::~ImeControllerClientImpl() {
  ime_controller_->SetClient(nullptr);
  DCHECK_EQ(this, g_ime_controller_client_instance);
  g_ime_controller_client_instance = nullptr;

  InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  input_method_manager_->RemoveImeMenuObserver(this);
  input_method_manager_->RemoveObserver(this);
}

void ImeControllerClientImpl::Init() {
  // Connect to the controller in ash.
  ime_controller_ = ash::ImeController::Get();
  DCHECK(ime_controller_);
  InitAndSetClient();
}

// static
ImeControllerClientImpl* ImeControllerClientImpl::Get() {
  return g_ime_controller_client_instance;
}

void ImeControllerClientImpl::SetImesManagedByPolicy(bool managed) {
  ime_controller_->SetImesManagedByPolicy(managed);
}

// ash::mojom::ImeControllerClient:
void ImeControllerClientImpl::SwitchToNextIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->SwitchToNextInputMethod();
}

void ImeControllerClientImpl::SwitchToLastUsedIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->SwitchToLastUsedInputMethod();
}

void ImeControllerClientImpl::SwitchImeById(const std::string& id,
                                            bool show_message) {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->ChangeInputMethod(id, show_message);
}

void ImeControllerClientImpl::ActivateImeMenuItem(const std::string& key) {
  input_method_manager_->ActivateInputMethodMenuItem(key);
}

void ImeControllerClientImpl::SetCapsLockEnabled(bool caps_enabled) {
  ash::input_method::ImeKeyboard* keyboard =
      InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->SetCapsLockEnabled(caps_enabled);
}

void ImeControllerClientImpl::OverrideKeyboardKeyset(
    ash::input_method::ImeKeyset keyset,
    OverrideKeyboardKeysetCallback callback) {
  input_method_manager_->OverrideKeyboardKeyset(keyset);
  std::move(callback).Run();
}

void ImeControllerClientImpl::ShowModeIndicator() {
  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor =
      input_method_manager_->GetActiveIMEState()->GetCurrentInputMethod();
  const std::u16string short_name = descriptor.GetIndicator();

  ash::IMECandidateWindowHandlerInterface* cw_handler =
      ash::IMEBridge::Get()->GetCandidateWindowHandler();
  if (!cw_handler)
    return;

  gfx::Rect anchor_bounds = cw_handler->GetCursorBounds();
  if (anchor_bounds == gfx::Rect()) {
    // TODO(shuchen): Show the mode indicator in the right bottom of the
    // display when the launch bar is hidden and the focus is out.  To
    // implement it, we should consider to use message center or system
    // notification.  Note, launch bar can be vertical and can be placed
    // right/left side of display.
    return;
  }

  // Call to Ash to show the mode indicator view with the given anchor
  // bounds and short name.
  ime_controller_->ShowModeIndicator(anchor_bounds, short_name);
}

// ash::input_method::InputMethodManager::Observer:
void ImeControllerClientImpl::InputMethodChanged(InputMethodManager* manager,
                                                 Profile* profile,
                                                 bool show_message) {
  RefreshIme();
  if (show_message)
    ShowModeIndicator();
}

// ash::input_method::InputMethodManager::ImeMenuObserver:
void ImeControllerClientImpl::ImeMenuActivationChanged(bool is_active) {
  ime_controller_->ShowImeMenuOnShelf(is_active);
}

void ImeControllerClientImpl::ImeMenuListChanged() {
  RefreshIme();
}

void ImeControllerClientImpl::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

// ui::ime::InputMethodMenuManager::Observer:
void ImeControllerClientImpl::InputMethodMenuItemChanged(
    InputMethodMenuManager* manager) {
  RefreshIme();
}

// ash::input_method::ImeKeyboard::Observer:
void ImeControllerClientImpl::OnCapsLockChanged(bool enabled) {
  ime_controller_->UpdateCapsLockState(enabled);
}

void ImeControllerClientImpl::OnLayoutChanging(const std::string& layout_name) {
  ime_controller_->OnKeyboardLayoutNameChanged(layout_name);
}

void ImeControllerClientImpl::InitAndSetClient() {
  ime_controller_->SetClient(this);

  // Now that the client is set, flush state from observed objects to
  // the ImeController, now that it will hear it.
  input_method_manager_->NotifyObserversImeExtraInputStateChange();
  if (const ash::input_method::ImeKeyboard* keyboard =
          input_method_manager_->GetImeKeyboard()) {
    ime_controller_->OnKeyboardLayoutNameChanged(
        keyboard->GetCurrentKeyboardLayoutName());
  }
}

ash::ImeInfo ImeControllerClientImpl::GetAshImeInfo(
    const InputMethodDescriptor& ime) const {
  InputMethodUtil* util = input_method_manager_->GetInputMethodUtil();
  ash::ImeInfo info;
  info.id = ime.id();
  info.name = util->GetInputMethodLongName(ime);
  info.short_name = ime.GetIndicator();
  info.third_party = ash::extension_ime_util::IsExtensionIME(ime.id());
  return info;
}

void ImeControllerClientImpl::RefreshIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (!state) {
    const std::string empty_ime_id;
    ime_controller_->RefreshIme(empty_ime_id, std::vector<ash::ImeInfo>(),
                                std::vector<ash::ImeMenuItem>());
    return;
  }

  const std::string current_ime_id = state->GetCurrentInputMethod().id();

  std::vector<ash::ImeInfo> available_imes;
  std::vector<InputMethodDescriptor> enabled_ime_descriptors =
      state->GetEnabledInputMethodsSortedByLocalizedDisplayNames();
  for (const InputMethodDescriptor& descriptor : enabled_ime_descriptors) {
    ash::ImeInfo info = GetAshImeInfo(descriptor);
    available_imes.push_back(std::move(info));
  }

  std::vector<ash::ImeMenuItem> ash_menu_items;
  ui::ime::InputMethodMenuItemList menu_list =
      ui::ime::InputMethodMenuManager::GetInstance()
          ->GetCurrentInputMethodMenuItemList();
  for (const ui::ime::InputMethodMenuItem& menu_item : menu_list) {
    ash::ImeMenuItem ash_item;
    ash_item.key = menu_item.key;
    ash_item.label = base::UTF8ToUTF16(menu_item.label);
    ash_item.checked = menu_item.is_selection_item_checked;
    ash_menu_items.push_back(std::move(ash_item));
  }
  ime_controller_->RefreshIme(current_ime_id, std::move(available_imes),
                              std::move(ash_menu_items));
}

void ImeControllerClientImpl::OnExtraInputEnabledStateChange(
    bool is_extra_input_options_enabled,
    bool is_emoji_enabled,
    bool is_handwriting_enabled,
    bool is_voice_enabled) {
  if (ime_controller_) {
    ime_controller_->SetExtraInputOptionsEnabledState(
        is_extra_input_options_enabled, is_emoji_enabled,
        is_handwriting_enabled, is_voice_enabled);
  }
}
