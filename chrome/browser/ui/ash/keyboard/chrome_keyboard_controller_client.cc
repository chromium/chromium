// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_web_contents.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

namespace {

static ChromeKeyboardControllerClient* g_chrome_keyboard_controller_client =
    nullptr;

}  // namespace

// static
std::unique_ptr<ChromeKeyboardControllerClient>
ChromeKeyboardControllerClient::Create() {
  // Use WrapUnique to allow the constructor to be private.
  std::unique_ptr<ChromeKeyboardControllerClient> client =
      base::WrapUnique(new ChromeKeyboardControllerClient());
  client->InitializePrefObserver();
  return client;
}

// static
std::unique_ptr<ChromeKeyboardControllerClient>
ChromeKeyboardControllerClient::CreateForTest() {
  // Use WrapUnique to allow the constructor to be private.
  return base::WrapUnique(new ChromeKeyboardControllerClient());
}

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

ChromeKeyboardControllerClient::ChromeKeyboardControllerClient() {
  CHECK(!g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = this;
}

void ChromeKeyboardControllerClient::Init(
    ash::KeyboardController* keyboard_controller) {
  DCHECK(!keyboard_controller_);
  keyboard_controller_ = keyboard_controller;

  // Add this as a KeyboardController observer.
  keyboard_controller_->AddObserver(this);

  // Request the initial enabled state.
  OnKeyboardEnabledChanged(keyboard_controller_->IsKeyboardEnabled());

  // Request the initial set of enable flags.
  OnKeyboardEnableFlagsChanged(keyboard_controller_->GetEnableFlags());

  // Request the initial visible state.
  OnKeyboardVisibilityChanged(keyboard_controller_->IsKeyboardVisible());

  // Request the configuration.
  OnKeyboardConfigChanged(keyboard_controller_->GetKeyboardConfig());
}

ChromeKeyboardControllerClient::~ChromeKeyboardControllerClient() {
  CHECK(g_chrome_keyboard_controller_client);
  Shutdown();
  // Clear the global instance pointer last so that  keyboard_contents_ and
  // KeyboardController owned classes can remove themselves as observers.
  g_chrome_keyboard_controller_client = nullptr;
}

void ChromeKeyboardControllerClient::InitializePrefObserver() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

void ChromeKeyboardControllerClient::Shutdown() {
  if (keyboard_controller_) {
    keyboard_controller_->RemoveObserver(this);
    keyboard_controller_ = nullptr;
  }

  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);
  pref_change_registrar_.reset();

  if (keyboard::KeyboardUIController::HasInstance()) {
    // In classic Ash, keyboard::KeyboardController owns ChromeKeyboardUI which
    // accesses this class, so make sure that the UI has been destroyed.
    keyboard::KeyboardUIController::Get()->Shutdown();
  }
  keyboard_contents_.reset();
}

void ChromeKeyboardControllerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeKeyboardControllerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ChromeKeyboardControllerClient::NotifyKeyboardLoaded() {
  DVLOG(1) << "NotifyKeyboardLoaded: " << is_keyboard_loaded_;
  is_keyboard_loaded_ = true;
  for (auto& observer : observers_)
    observer.OnKeyboardLoaded();
}

keyboard::KeyboardConfig ChromeKeyboardControllerClient::GetKeyboardConfig() {
  if (!cached_keyboard_config_) {
    // Unlikely edge case (called before the Ash mojo service replies to the
    // initial GetKeyboardConfig request). Return the default value.
    return keyboard::KeyboardConfig();
  }
  return *cached_keyboard_config_;
}

void ChromeKeyboardControllerClient::SetKeyboardConfig(
    const keyboard::KeyboardConfig& config) {
  // Update the cache immediately.
  cached_keyboard_config_ = config;
  keyboard_controller_->SetKeyboardConfig(config);
}

bool ChromeKeyboardControllerClient::GetKeyboardEnabled() {
  // |keyboard_controller_| may be null during shutdown.
  return keyboard_controller_ ? keyboard_controller_->IsKeyboardEnabled()
                              : false;
}

void ChromeKeyboardControllerClient::SetEnableFlag(
    const keyboard::KeyboardEnableFlag& flag) {
  DVLOG(1) << "SetEnableFlag: " << static_cast<int>(flag);
  keyboard_controller_->SetEnableFlag(flag);
}

void ChromeKeyboardControllerClient::ClearEnableFlag(
    const keyboard::KeyboardEnableFlag& flag) {
  keyboard_controller_->ClearEnableFlag(flag);
}

bool ChromeKeyboardControllerClient::IsEnableFlagSet(
    const keyboard::KeyboardEnableFlag& flag) {
  return base::Contains(keyboard_enable_flags_, flag);
}

void ChromeKeyboardControllerClient::ReloadKeyboardIfNeeded() {
  // |keyboard_controller_| may be null if the keyboard reloads during shutdown.
  if (keyboard_controller_)
    keyboard_controller_->ReloadKeyboardIfNeeded();
}

void ChromeKeyboardControllerClient::RebuildKeyboardIfEnabled() {
  keyboard_controller_->RebuildKeyboardIfEnabled();
}

void ChromeKeyboardControllerClient::ShowKeyboard() {
  keyboard_controller_->ShowKeyboard();
}

void ChromeKeyboardControllerClient::HideKeyboard(ash::HideReason reason) {
  keyboard_controller_->HideKeyboard(reason);
}

void ChromeKeyboardControllerClient::SetContainerType(
    keyboard::ContainerType container_type,
    const gfx::Rect& target_bounds,
    base::OnceCallback<void(bool)> callback) {
  keyboard_controller_->SetContainerType(container_type, target_bounds,
                                         std::move(callback));
}

void ChromeKeyboardControllerClient::SetKeyboardLocked(bool locked) {
  keyboard_controller_->SetKeyboardLocked(locked);
}

void ChromeKeyboardControllerClient::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_->SetOccludedBounds(bounds);
}

void ChromeKeyboardControllerClient::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_->SetHitTestBounds(bounds);
}

bool ChromeKeyboardControllerClient::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  return keyboard_controller_->SetAreaToRemainOnScreen(bounds);
}

void ChromeKeyboardControllerClient::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_controller_->SetDraggableArea(bounds);
}

bool ChromeKeyboardControllerClient::SetWindowBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  return keyboard_controller_->SetWindowBoundsInScreen(bounds_in_screen);
}

void ChromeKeyboardControllerClient::SetKeyboardConfigFromPref(bool enabled) {
  keyboard_controller_->SetKeyboardConfigFromPref(enabled);
}

bool ChromeKeyboardControllerClient::IsKeyboardOverscrollEnabled() {
  return keyboard_controller_->ShouldOverscroll();
}

GURL ChromeKeyboardControllerClient::GetVirtualKeyboardUrl() {
  if (!virtual_keyboard_url_for_test_.is_empty())
    return virtual_keyboard_url_for_test_;

  auto* ime_manager = ash::input_method::InputMethodManager::Get();
  if (!ime_manager || !ime_manager->GetActiveIMEState())
    return GURL(keyboard::kKeyboardURL);

  const GURL& input_view_url =
      ime_manager->GetActiveIMEState()->GetInputViewUrl();
  if (!input_view_url.is_valid())
    return GURL(keyboard::kKeyboardURL);

  return input_view_url;
}

aura::Window* ChromeKeyboardControllerClient::GetKeyboardWindow() const {
  return keyboard::KeyboardUIController::Get()->GetKeyboardWindow();
}

void ChromeKeyboardControllerClient::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  keyboard_enable_flags_ = flags;
}

void ChromeKeyboardControllerClient::OnKeyboardEnabledChanged(bool enabled) {
  DVLOG(1) << "OnKeyboardEnabledChanged: " << enabled;

  bool was_enabled = is_keyboard_enabled_;
  is_keyboard_enabled_ = enabled;

  for (auto& observer : observers_)
    observer.OnKeyboardEnabledChanged(is_keyboard_enabled_);

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
      base::Value::List(), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardConfigChanged(
    const keyboard::KeyboardConfig& config) {
  // Only notify extensions after the initial config is received.
  bool notify = !!cached_keyboard_config_;
  cached_keyboard_config_ = std::move(config);
  if (!notify)
    return;
  extensions::VirtualKeyboardAPI* api =
      extensions::BrowserContextKeyedAPIFactory<
          extensions::VirtualKeyboardAPI>::Get(GetProfile());
  api->delegate()->OnKeyboardConfigChanged();
}

void ChromeKeyboardControllerClient::OnKeyboardVisibilityChanged(bool visible) {
  is_keyboard_visible_ = visible;
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(visible);
}

void ChromeKeyboardControllerClient::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardVisibleBoundsChanged: " << screen_bounds.ToString();
  if (keyboard_contents_)
    keyboard_contents_->SetInitialContentsSize(screen_bounds.size());

  for (auto& observer : observers_) {
    observer.OnKeyboardVisibleBoundsChanged(screen_bounds);
  }

  if (!GetKeyboardWindow())
    return;

  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  // |router| may be null in tests.
  if (!router || !router->HasEventListener(
                     virtual_keyboard_private::OnBoundsChanged::kEventName)) {
    return;
  }

  // Convert screen bounds to the frame of reference of the keyboard window.
  gfx::Rect bounds = BoundsFromScreen(screen_bounds);
  base::Value::List event_args;
  base::Value::Dict new_bounds;
  new_bounds.Set("left", bounds.x());
  new_bounds.Set("top", bounds.y());
  new_bounds.Set("width", bounds.width());
  new_bounds.Set("height", bounds.height());
  event_args.Append(std::move(new_bounds));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
      virtual_keyboard_private::OnBoundsChanged::kEventName,
      std::move(event_args), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  if (!GetKeyboardWindow())
    return;
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardOccludedBoundsChanged(screen_bounds);
}

void ChromeKeyboardControllerClient::OnLoadKeyboardContentsRequested() {
  GURL keyboard_url = GetVirtualKeyboardUrl();
  if (keyboard_contents_) {
    DVLOG(1) << "OnLoadKeyboardContentsRequested: SetUrl: " << keyboard_url;
    keyboard_contents_->SetKeyboardUrl(keyboard_url);
    return;
  }

  DVLOG(1) << "OnLoadKeyboardContentsRequested: Create: " << keyboard_url;
  keyboard_contents_ = std::make_unique<ChromeKeyboardWebContents>(
      GetProfile(), keyboard_url,
      /*load_callback=*/
      base::BindOnce(&ChromeKeyboardControllerClient::OnKeyboardContentsLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      /*unembed_callback=*/
      base::BindRepeating(
          &ChromeKeyboardControllerClient::OnKeyboardUIDestroyed,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChromeKeyboardControllerClient::OnKeyboardUIDestroyed() {
  keyboard_contents_.reset();
}

void ChromeKeyboardControllerClient::OnKeyboardContentsLoaded() {
  DVLOG(1) << "OnKeyboardContentsLoaded";
  NotifyKeyboardLoaded();
}

void ChromeKeyboardControllerClient::OnSessionStateChanged() {
  TRACE_EVENT0("login",
               "ChromeKeyboardControllerClient::OnSessionStateChanged");
  if (base::FeatureList::IsEnabled(
          ash::features::kTouchVirtualKeyboardPolicyListenPrefsAtLogin)) {
    // We need to listen for pref changes even in login screen to control the
    // virtual keyboard behavior on the login screen.
    pref_change_registrar_.reset();
  } else {
    if (!session_manager::SessionManager::Get()->IsSessionStarted()) {
      // Reset the registrar so that prefs are re-registered after a crash.
      pref_change_registrar_.reset();
      return;
    }
    if (pref_change_registrar_) {
      return;
    }
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kTouchVirtualKeyboardEnabled,
      base::BindRepeating(
          &ChromeKeyboardControllerClient::SetTouchKeyboardEnabledFromPrefs,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kVirtualKeyboardSmartVisibilityEnabled,
      base::BindRepeating(
          &ChromeKeyboardControllerClient::SetSmartVisibilityFromPrefs,
          base::Unretained(this)));

  SetTouchKeyboardEnabledFromPrefs();
  SetSmartVisibilityFromPrefs();
}

void ChromeKeyboardControllerClient::SetTouchKeyboardEnabledFromPrefs() {
  using keyboard::KeyboardEnableFlag;
  const PrefService* service = pref_change_registrar_->prefs();
  if (service->HasPrefPath(prefs::kTouchVirtualKeyboardEnabled)) {
    // Since these flags are mutually exclusive, setting one clears the other.
    SetEnableFlag(service->GetBoolean(prefs::kTouchVirtualKeyboardEnabled)
                      ? KeyboardEnableFlag::kPolicyEnabled
                      : KeyboardEnableFlag::kPolicyDisabled);
  } else {
    ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
    ClearEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
  }
}

void ChromeKeyboardControllerClient::SetSmartVisibilityFromPrefs() {
  const PrefService* service = pref_change_registrar_->prefs();
  if (service->HasPrefPath(prefs::kVirtualKeyboardSmartVisibilityEnabled)) {
    keyboard_controller_->SetSmartVisibilityEnabled(
        service->GetBoolean(prefs::kVirtualKeyboardSmartVisibilityEnabled));
  }
}

Profile* ChromeKeyboardControllerClient::GetProfile() {
  if (profile_for_test_)
    return profile_for_test_;

  // Always use the active profile for generating keyboard events so that any
  // virtual keyboard extensions associated with the active user are notified.
  // (Note: UI and associated extensions only exist for the active user).
  return ProfileManager::GetActiveUserProfile();
}

gfx::Rect ChromeKeyboardControllerClient::BoundsFromScreen(
    const gfx::Rect& screen_bounds) {
  aura::Window* keyboard_window = GetKeyboardWindow();
  DCHECK(keyboard_window);
  gfx::Rect bounds(screen_bounds);
  ::wm::ConvertRectFromScreen(keyboard_window, &bounds);
  return bounds;
}
