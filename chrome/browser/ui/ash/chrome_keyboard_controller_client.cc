// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"

#include <memory>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "extensions/common/extension_messages.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_resource_util.h"
#include "ui/keyboard/keyboard_switches.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

namespace {

static ChromeKeyboardControllerClient* g_chrome_keyboard_controller_client =
    nullptr;

}  // namespace

// static
ChromeKeyboardControllerClient* ChromeKeyboardControllerClient::Get() {
  CHECK(g_chrome_keyboard_controller_client)
      << "ChromeKeyboardControllerClient::Get() called before Initialize()";
  return g_chrome_keyboard_controller_client;
}

// static
bool ChromeKeyboardControllerClient::HasInstance() {
  return !!g_chrome_keyboard_controller_client;
}

ChromeKeyboardControllerClient::ChromeKeyboardControllerClient(
    service_manager::Connector* connector) {
  CHECK(!g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = this;

  if (!connector)
    return;  // May be null in tests.

  connector->BindInterface(ash::mojom::kServiceName, &keyboard_controller_ptr_);

  // Request the configuration. This will be queued until the service is ready.
  keyboard_controller_ptr_->GetKeyboardConfig(base::BindOnce(
      &ChromeKeyboardControllerClient::OnGetInitialKeyboardConfig,
      weak_ptr_factory_.GetWeakPtr()));
}

ChromeKeyboardControllerClient::~ChromeKeyboardControllerClient() {
  CHECK(g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = nullptr;
}

void ChromeKeyboardControllerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeKeyboardControllerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

keyboard::mojom::KeyboardConfig
ChromeKeyboardControllerClient::GetKeyboardConfig() {
  if (!cached_keyboard_config_) {
    // Unlikely edge case (called before the Ash mojo service replies to the
    // initial GetKeyboardConfig request). Return the default value.
    return keyboard::mojom::KeyboardConfig();
  }
  return *cached_keyboard_config_.get();
}

void ChromeKeyboardControllerClient::SetKeyboardConfig(
    const keyboard::mojom::KeyboardConfig& config) {
  // Update the cache immediately.
  cached_keyboard_config_ = keyboard::mojom::KeyboardConfig::New(config);
  keyboard_controller_ptr_->SetKeyboardConfig(cached_keyboard_config_.Clone());
}

void ChromeKeyboardControllerClient::SetEnableFlag(
    const keyboard::mojom::KeyboardEnableFlag& flag) {
  keyboard_controller_ptr_->SetEnableFlag(flag);
}

void ChromeKeyboardControllerClient::ClearEnableFlag(
    const keyboard::mojom::KeyboardEnableFlag& flag) {
  keyboard_controller_ptr_->ClearEnableFlag(flag);
}

void ChromeKeyboardControllerClient::ReloadKeyboard() {
  keyboard_controller_ptr_->ReloadKeyboard();
}

bool ChromeKeyboardControllerClient::IsKeyboardOverscrollEnabled() {
  DCHECK(cached_keyboard_config_);
  if (cached_keyboard_config_->overscroll_behavior !=
      keyboard::mojom::KeyboardOverscrollBehavior::kDefault) {
    return cached_keyboard_config_->overscroll_behavior ==
           keyboard::mojom::KeyboardOverscrollBehavior::kEnabled;
  }
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      keyboard::switches::kDisableVirtualKeyboardOverscroll);
}

GURL ChromeKeyboardControllerClient::GetVirtualKeyboardUrl() {
  if (!virtual_keyboard_url_for_test_.is_empty())
    return virtual_keyboard_url_for_test_;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kDisableInputView)) {
    return GURL(keyboard::kKeyboardURL);
  }

  chromeos::input_method::InputMethodManager* ime_manager =
      chromeos::input_method::InputMethodManager::Get();
  if (!ime_manager || !ime_manager->GetActiveIMEState())
    return GURL(keyboard::kKeyboardURL);

  const GURL& input_view_url =
      ime_manager->GetActiveIMEState()->GetInputViewUrl();
  if (!input_view_url.is_valid())
    return GURL(keyboard::kKeyboardURL);

  return input_view_url;
}

void ChromeKeyboardControllerClient::FlushForTesting() {
  keyboard_controller_ptr_.FlushForTesting();
}

void ChromeKeyboardControllerClient::OnGetInitialKeyboardConfig(
    keyboard::mojom::KeyboardConfigPtr config) {
  // Only set the cached value if not already set by SetKeyboardConfig (the
  // set value will override the initial value once processed).
  if (!cached_keyboard_config_)
    cached_keyboard_config_ = std::move(config);

  // Add this as a KeyboardController observer now that the service is ready.
  ash::mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
  keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
  keyboard_controller_ptr_->AddObserver(std::move(ptr_info));

  // Request the initial enabled state.
  keyboard_controller_ptr_->IsKeyboardEnabled(
      base::BindOnce(&ChromeKeyboardControllerClient::OnKeyboardEnabledChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeKeyboardControllerClient::OnKeyboardEnabledChanged(bool enabled) {
  bool was_enabled = is_keyboard_enabled_;
  is_keyboard_enabled_ = enabled;
  if (enabled || !was_enabled)
    return;

  // When the keyboard becomes disabled, send the onKeyboardClosed event.

  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  // |router| may be null in tests.
  if (!router || !router->HasEventListener(
                     virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
      virtual_keyboard_private::OnKeyboardClosed::kEventName,
      std::make_unique<base::ListValue>(), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardConfigChanged(
    keyboard::mojom::KeyboardConfigPtr config) {
  cached_keyboard_config_ = std::move(config);
  extensions::VirtualKeyboardAPI* api =
      extensions::BrowserContextKeyedAPIFactory<
          extensions::VirtualKeyboardAPI>::Get(GetProfile());
  api->delegate()->OnKeyboardConfigChanged();
}

void ChromeKeyboardControllerClient::OnKeyboardVisibilityChanged(bool visible) {
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(visible);
}

void ChromeKeyboardControllerClient::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  // |router| may be null in tests.
  if (!router || !router->HasEventListener(
                     virtual_keyboard_private::OnBoundsChanged::kEventName)) {
    return;
  }

  auto event_args = std::make_unique<base::ListValue>();
  auto new_bounds = std::make_unique<base::DictionaryValue>();
  new_bounds->SetInteger("left", bounds.x());
  new_bounds->SetInteger("top", bounds.y());
  new_bounds->SetInteger("width", bounds.width());
  new_bounds->SetInteger("height", bounds.height());
  event_args->Append(std::move(new_bounds));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
      virtual_keyboard_private::OnBoundsChanged::kEventName,
      std::move(event_args), profile);
  router->BroadcastEvent(std::move(event));
}

Profile* ChromeKeyboardControllerClient::GetProfile() {
  if (profile_for_test_)
    return profile_for_test_;

  // Always use the active profile for generating keyboard events so that any
  // virtual keyboard extensions associated with the active user are notified.
  // (Note: UI and associated extensions only exist for the active user).
  return ProfileManager::GetActiveUserProfile();
}
