// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_client.h"

#include <memory>
#include <vector>

#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/ime_info.mojom.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/ime_bridge.h"

using chromeos::input_method::InputMethodDescriptor;
using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using ui::ime::InputMethodMenuManager;

namespace {

ImeControllerClient* g_ime_controller_client_instance = nullptr;

}  // namespace

ImeControllerClient::ImeControllerClient(InputMethodManager* manager)
    : input_method_manager_(manager) {
  DCHECK(input_method_manager_);
  input_method_manager_->AddObserver(this);
  input_method_manager_->AddImeMenuObserver(this);
  if (input_method_manager_->GetImeKeyboard())
    input_method_manager_->GetImeKeyboard()->AddObserver(this);
  InputMethodMenuManager::GetInstance()->AddObserver(this);

  // This does not need to send the initial state to ash because that happens
  // via observers when the InputMethodManager initializes its list of IMEs.

  DCHECK(!g_ime_controller_client_instance);
  g_ime_controller_client_instance = this;
}

ImeControllerClient::~ImeControllerClient() {
  DCHECK_EQ(this, g_ime_controller_client_instance);
  g_ime_controller_client_instance = nullptr;

  InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  input_method_manager_->RemoveImeMenuObserver(this);
  input_method_manager_->RemoveObserver(this);
  if (input_method_manager_->GetImeKeyboard())
    input_method_manager_->GetImeKeyboard()->RemoveObserver(this);
}

void ImeControllerClient::Init() {
  // Connect to the controller in ash.
  content::GetSystemConnector()->Connect(
      ash::mojom::kServiceName, ime_controller_.BindNewPipeAndPassReceiver());
  BindAndSetClient();
}

void ImeControllerClient::InitForTesting(
    mojo::PendingRemote<ash::mojom::ImeController> controller) {
  ime_controller_.Bind(std::move(controller));
  BindAndSetClient();
}

// static
ImeControllerClient* ImeControllerClient::Get() {
  return g_ime_controller_client_instance;
}

void ImeControllerClient::SetImesManagedByPolicy(bool managed) {
  ime_controller_->SetImesManagedByPolicy(managed);
}

// ash::mojom::ImeControllerClient:
void ImeControllerClient::SwitchToNextIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->SwitchToNextInputMethod();
}

void ImeControllerClient::SwitchToLastUsedIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->SwitchToLastUsedInputMethod();
}

void ImeControllerClient::SwitchImeById(const std::string& id,
                                        bool show_message) {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (state)
    state->ChangeInputMethod(id, show_message);
}

void ImeControllerClient::ActivateImeMenuItem(const std::string& key) {
  input_method_manager_->ActivateInputMethodMenuItem(key);
}

void ImeControllerClient::SetCapsLockEnabled(bool caps_enabled) {
  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->SetCapsLockEnabled(caps_enabled);
}

void ImeControllerClient::UpdateMirroringState(bool mirroring_enabled) {
  ui::IMEEngineHandlerInterface* ime_engine =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  if (ime_engine)
    ime_engine->SetMirroringEnabled(mirroring_enabled);
}

void ImeControllerClient::UpdateCastingState(bool casting_enabled) {
  ui::IMEEngineHandlerInterface* ime_engine =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  if (ime_engine)
    ime_engine->SetCastingEnabled(casting_enabled);
}

void ImeControllerClient::OverrideKeyboardKeyset(
    chromeos::input_method::mojom::ImeKeyset keyset,
    OverrideKeyboardKeysetCallback callback) {
  input_method_manager_->OverrideKeyboardKeyset(keyset);
  std::move(callback).Run();
}

void ImeControllerClient::ShowModeIndicator() {
  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor =
      input_method_manager_->GetActiveIMEState()->GetCurrentInputMethod();
  const base::string16 short_name =
      input_method_manager_->GetInputMethodUtil()->GetInputMethodShortName(
          descriptor);

  chromeos::IMECandidateWindowHandlerInterface* cw_handler =
      ui::IMEBridge::Get()->GetCandidateWindowHandler();
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

  // Mojo call to Ash to show the mode indicator view with the given anchor
  // bounds and short name.
  ime_controller_->ShowModeIndicator(anchor_bounds, short_name);
}

// chromeos::input_method::InputMethodManager::Observer:
void ImeControllerClient::InputMethodChanged(InputMethodManager* manager,
                                             Profile* profile,
                                             bool show_message) {
  RefreshIme();
  if (show_message)
    ShowModeIndicator();
}

// chromeos::input_method::InputMethodManager::ImeMenuObserver:
void ImeControllerClient::ImeMenuActivationChanged(bool is_active) {
  ime_controller_->ShowImeMenuOnShelf(is_active);
}

void ImeControllerClient::ImeMenuListChanged() {
  RefreshIme();
}

void ImeControllerClient::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

// ui::ime::InputMethodMenuManager::Observer:
void ImeControllerClient::InputMethodMenuItemChanged(
    InputMethodMenuManager* manager) {
  RefreshIme();
}

// chromeos::input_method::ImeKeyboard::Observer:
void ImeControllerClient::OnCapsLockChanged(bool enabled) {
  ime_controller_->UpdateCapsLockState(enabled);
}

void ImeControllerClient::OnLayoutChanging(const std::string& layout_name) {
  ime_controller_->OnKeyboardLayoutNameChanged(layout_name);
}

void ImeControllerClient::FlushMojoForTesting() {
  ime_controller_.FlushForTesting();
}

void ImeControllerClient::BindAndSetClient() {
  mojo::PendingRemote<ash::mojom::ImeControllerClient> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  ime_controller_->SetClient(std::move(client));

  // Now that the bridge is established, flush state from observed objects to
  // the ImeController, now that it will hear it.
  input_method_manager_->NotifyObserversImeExtraInputStateChange();
  if (const chromeos::input_method::ImeKeyboard* keyboard =
          input_method_manager_->GetImeKeyboard()) {
    ime_controller_->OnKeyboardLayoutNameChanged(
        keyboard->GetCurrentKeyboardLayoutName());
  }
}

ash::mojom::ImeInfoPtr ImeControllerClient::GetAshImeInfo(
    const InputMethodDescriptor& ime) const {
  InputMethodUtil* util = input_method_manager_->GetInputMethodUtil();
  ash::mojom::ImeInfoPtr info = ash::mojom::ImeInfo::New();
  info->id = ime.id();
  info->name = util->GetInputMethodLongName(ime);
  info->medium_name = util->GetInputMethodMediumName(ime);
  info->short_name = util->GetInputMethodShortName(ime);
  info->third_party = chromeos::extension_ime_util::IsExtensionIME(ime.id());
  return info;
}

void ImeControllerClient::RefreshIme() {
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (!state) {
    const std::string empty_ime_id;
    ime_controller_->RefreshIme(empty_ime_id,
                                std::vector<ash::mojom::ImeInfoPtr>(),
                                std::vector<ash::mojom::ImeMenuItemPtr>());
    return;
  }

  const std::string current_ime_id = state->GetCurrentInputMethod().id();

  std::vector<ash::mojom::ImeInfoPtr> available_imes;
  std::unique_ptr<std::vector<InputMethodDescriptor>>
      available_ime_descriptors = state->GetActiveInputMethods();
  for (const InputMethodDescriptor& descriptor : *available_ime_descriptors) {
    ash::mojom::ImeInfoPtr info = GetAshImeInfo(descriptor);
    available_imes.push_back(std::move(info));
  }

  std::vector<ash::mojom::ImeMenuItemPtr> ash_menu_items;
  ui::ime::InputMethodMenuItemList menu_list =
      ui::ime::InputMethodMenuManager::GetInstance()
          ->GetCurrentInputMethodMenuItemList();
  for (const ui::ime::InputMethodMenuItem& menu_item : menu_list) {
    ash::mojom::ImeMenuItemPtr ash_item = ash::mojom::ImeMenuItem::New();
    ash_item->key = menu_item.key;
    ash_item->label = base::UTF8ToUTF16(menu_item.label);
    ash_item->checked = menu_item.is_selection_item_checked;
    ash_menu_items.push_back(std::move(ash_item));
  }
  ime_controller_->RefreshIme(current_ime_id, std::move(available_imes),
                              std::move(ash_menu_items));
}

void ImeControllerClient::OnExtraInputEnabledStateChange(
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
