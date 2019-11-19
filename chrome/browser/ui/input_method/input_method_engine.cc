// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine.h"
#include "base/stl_util.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "url/gurl.h"

namespace {

const char kErrorFollowCursorWindowExists[] =
    "A follow cursor IME window exists.";
const char kErrorNoInputFocus[] =
    "The follow cursor IME window cannot be created without an input focus.";
const char kErrorReachMaxWindowCount[] =
    "Cannot create more than 5 normal IME windows.";

const int kMaxNormalWindowCount = 5;

}  // namespace

namespace input_method {

InputMethodEngine::InputMethodEngine() : follow_cursor_window_(nullptr) {}

InputMethodEngine::~InputMethodEngine() {
  // Removes the listeners for OnWindowDestroyed.
  if (follow_cursor_window_)
    follow_cursor_window_->RemoveObserver(this);
  for (auto* window : normal_windows_)
    window->RemoveObserver(this);

  CloseImeWindows();
}

void InputMethodEngine::FocusOut() {
  InputMethodEngineBase::FocusOut();
  if (follow_cursor_window_)
    follow_cursor_window_->Hide();
}

void InputMethodEngine::SetCompositionBounds(
    const std::vector<gfx::Rect>& bounds) {
  InputMethodEngineBase::SetCompositionBounds(bounds);
  if (!bounds.empty()) {
    current_cursor_bounds_ = bounds[0];
    if (follow_cursor_window_)
      follow_cursor_window_->FollowCursor(current_cursor_bounds_);
  }
}

void InputMethodEngine::UpdateComposition(
    const ui::CompositionText& composition_text,
    uint32_t cursor_pos,
    bool is_visible) {
  composition_ = composition_text;

  // Use a thin underline with text color by default.
  if (composition_.ime_text_spans.empty()) {
    composition_.ime_text_spans.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, 0, composition_.text.length(),
        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  // If the IME extension is handling key event, hold the composition text
  // until the key event is handled.
  if (input_context && !handling_key_event_) {
    input_context->UpdateCompositionText(composition_, cursor_pos, is_visible);
    composition_ = ui::CompositionText();
  } else {
    composition_changed_ = true;
  }
}

bool InputMethodEngine::SetCompositionRange(
    uint32_t before,
    uint32_t after,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  // Not supported on non-Chrome OS platforms.
  return false;
}

bool InputMethodEngine::SetSelectionRange(uint32_t start, uint32_t end) {
  // Not supported on non-Chrome OS platforms.
  return false;
}

void InputMethodEngine::CommitTextToInputContext(int context_id,
                                                 const std::string& text) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  // If the IME extension is handling key event, hold the text until the key
  // event is handled.
  if (input_context && !handling_key_event_) {
    input_context->CommitText(text);
    text_ = "";
  } else {
    // Append the text to the buffer, as it allows committing text multiple
    // times when processing a key event.
    text_ += text;
    commit_text_changed_ = true;
  }
}

bool InputMethodEngine::SendKeyEvent(ui::KeyEvent* event,
                                     const std::string& code) {
  DCHECK(event);

  // input.ime.sendKeyEvents API is only allowed to work on text fields.
  if (current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  if (event->key_code() == ui::VKEY_UNKNOWN)
    event->set_key_code(ui::DomCodeToUsLayoutKeyboardCode(event->code()));

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return false;

  // ENTER et al. keys are allowed to work only on http:, https: etc.
  if (!IsValidKeyForAllPages(event)) {
    if (IsSpecialPage(input_context->GetInputMethod()))
      return false;
  }

  input_context->SendKeyEvent(event);
  return true;
}

bool InputMethodEngine::IsActive() const {
  return true;
}

std::string InputMethodEngine::GetExtensionId() const {
  return extension_id_;
}

int InputMethodEngine::CreateImeWindow(
    const extensions::Extension* extension,
    content::RenderFrameHost* render_frame_host,
    const std::string& url,
    ui::ImeWindow::Mode mode,
    const gfx::Rect& bounds,
    std::string* error) {
  if (mode == ui::ImeWindow::FOLLOW_CURSOR) {
    if (follow_cursor_window_) {
      *error = kErrorFollowCursorWindowExists;
      return 0;
    }
    if (current_input_type_ == ui::TEXT_INPUT_TYPE_NONE) {
      *error = kErrorNoInputFocus;
      return 0;
    }
  }

  if (mode == ui::ImeWindow::NORMAL &&
      normal_windows_.size() >= kMaxNormalWindowCount) {
    *error = kErrorReachMaxWindowCount;
    return 0;
  }

  // ui::ImeWindow manages its own lifetime.
  ui::ImeWindow* ime_window = new ui::ImeWindow(
      profile_, extension, render_frame_host, url, mode, bounds);
  ime_window->AddObserver(this);
  ime_window->Show();
  if (mode == ui::ImeWindow::FOLLOW_CURSOR) {
    follow_cursor_window_ = ime_window;
    ime_window->FollowCursor(current_cursor_bounds_);
  } else {
    normal_windows_.push_back(ime_window);
  }

  return ime_window->GetFrameId();
}

void InputMethodEngine::ShowImeWindow(int window_id) {
  ui::ImeWindow* ime_window = FindWindowById(window_id);
  if (ime_window)
    ime_window->Show();
}

void InputMethodEngine::HideImeWindow(int window_id) {
  ui::ImeWindow* ime_window = FindWindowById(window_id);
  if (ime_window)
    ime_window->Hide();
}

void InputMethodEngine::CloseImeWindows() {
  if (follow_cursor_window_)
    follow_cursor_window_->Close();
  for (auto* window : normal_windows_)
    window->Close();
  normal_windows_.clear();
}

void InputMethodEngine::OnWindowDestroyed(ui::ImeWindow* ime_window) {
  if (ime_window == follow_cursor_window_) {
    follow_cursor_window_ = nullptr;
  } else {
    auto it = std::find(
        normal_windows_.begin(), normal_windows_.end(), ime_window);
    if (it != normal_windows_.end())
      normal_windows_.erase(it);
  }
}

ui::ImeWindow* InputMethodEngine::FindWindowById(int window_id) const {
  if (follow_cursor_window_ &&
      follow_cursor_window_->GetFrameId() == window_id) {
    return follow_cursor_window_;
  }
  for (auto* ime_window : normal_windows_) {
    if (ime_window->GetFrameId() == window_id)
      return ime_window;
  }
  return nullptr;
}

bool InputMethodEngine::IsSpecialPage(ui::InputMethod* input_method) {
  Browser* browser = chrome::FindLastActive();
  DCHECK(browser);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return true;

  // If the input method get from last active browser is different from the
  // input method get from input context, it means we can't get the real url of
  // the input view, treats |url| as special page anyway for security concerns.
  if (browser->window()->GetNativeWindow()->GetHost()->GetInputMethod() !=
      input_method)
    return true;

  GURL url = web_contents->GetLastCommittedURL();
  // If we can't determine the last committed url, treat it as special.
  if (!url.is_valid())
    return true;

  // Checks if the last committed url has the whitelisted sheme.
  std::vector<const char*> whitelist_schemes{url::kFtpScheme, url::kHttpScheme,
                                             url::kHttpsScheme};
  for (auto* scheme : whitelist_schemes) {
    if (url.SchemeIs(scheme))
      return false;
  }

  // Checks if the last committed url is whitelisted url.
  url::Origin origin = url::Origin::Create(url);
  std::vector<GURL> whitelist_urls{GURL(url::kAboutBlankURL),
                                   GURL(chrome::kChromeUINewTabURL),
                                   GURL(chrome::kChromeSearchLocalNtpUrl)};
  for (const GURL& whitelist_url : whitelist_urls) {
    if (url::Origin::Create(whitelist_url).IsSameOriginWith(origin))
      return false;
  }
  return true;
}

bool InputMethodEngine::IsValidKeyForAllPages(ui::KeyEvent* ui_event) {
  // Whitelists all character keys except for Enter and Tab keys.
  std::vector<ui::KeyboardCode> invalid_character_keycodes{ui::VKEY_TAB,
                                                           ui::VKEY_RETURN};
  if (ui_event->GetDomKey().IsCharacter() && !ui_event->IsControlDown() &&
      !ui_event->IsCommandDown()) {
    return !base::Contains(invalid_character_keycodes, ui_event->key_code());
  }

  // Whitelists Backspace key and arrow keys.
  std::vector<ui::KeyboardCode> whitelist_keycodes{
      ui::VKEY_BACK, ui::VKEY_LEFT, ui::VKEY_RIGHT, ui::VKEY_UP, ui::VKEY_DOWN};
  return base::Contains(whitelist_keycodes, ui_event->key_code());
}

}  // namespace input_method
