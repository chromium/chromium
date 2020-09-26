// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_text_input_manager.h"

#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xkbcommon/xkbcommon.h>

#include "base/strings/utf_string_conversions.h"
#include "components/exo/display.h"
#include "components/exo/text_input.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/xkb_tracker.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"


namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// text_input_v1 interface:

class WaylandTextInputDelegate : public TextInput::Delegate {
 public:
  WaylandTextInputDelegate(wl_resource* text_input,
                           const XkbTracker* xkb_tracker,
                           SerialTracker* serial_tracker)
      : text_input_(text_input),
        xkb_tracker_(xkb_tracker),
        serial_tracker_(serial_tracker) {}
  ~WaylandTextInputDelegate() override = default;

  void set_surface(wl_resource* surface) { surface_ = surface; }

 private:
  wl_client* client() { return wl_resource_get_client(text_input_); }

  // TextInput::Delegate:
  void Activated() override {
    zwp_text_input_v1_send_enter(text_input_, surface_);
    wl_client_flush(client());
  }

  void Deactivated() override {
    zwp_text_input_v1_send_leave(text_input_);
    wl_client_flush(client());
  }

  void OnVirtualKeyboardVisibilityChanged(bool is_visible) override {
    zwp_text_input_v1_send_input_panel_state(text_input_, is_visible);
    wl_client_flush(client());
  }

  void SetCompositionText(const ui::CompositionText& composition) override {
    for (const auto& span : composition.ime_text_spans) {
      uint32_t style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT;
      switch (span.type) {
        case ui::ImeTextSpan::Type::kComposition:
          if (span.thickness == ui::ImeTextSpan::Thickness::kThick) {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT;
          } else {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE;
          }
          break;
        case ui::ImeTextSpan::Type::kSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION;
          break;
        case ui::ImeTextSpan::Type::kMisspellingSuggestion:
        case ui::ImeTextSpan::Type::kAutocorrect:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT;
          break;
      }
      const size_t start =
          OffsetFromUTF16Offset(composition.text, span.start_offset);
      const size_t end =
          OffsetFromUTF16Offset(composition.text, span.end_offset);
      zwp_text_input_v1_send_preedit_styling(text_input_, start, end - start,
                                             style);
    }

    const size_t pos =
        OffsetFromUTF16Offset(composition.text, composition.selection.start());
    zwp_text_input_v1_send_preedit_cursor(text_input_, pos);

    const std::string utf8 = base::UTF16ToUTF8(composition.text);
    zwp_text_input_v1_send_preedit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        utf8.c_str(), utf8.c_str());

    wl_client_flush(client());
  }

  void Commit(const base::string16& text) override {
    zwp_text_input_v1_send_commit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        base::UTF16ToUTF8(text).c_str());
    wl_client_flush(client());
  }

  void SetCursor(const gfx::Range& selection) override {
    zwp_text_input_v1_send_cursor_position(text_input_, selection.end(),
                                           selection.start());
  }

  void DeleteSurroundingText(const gfx::Range& range) override {
    zwp_text_input_v1_send_delete_surrounding_text(text_input_, range.start(),
                                                   range.length());
  }

  void SendKey(const ui::KeyEvent& event) override {
    uint32_t keysym = xkb_tracker_->GetKeysym(
        ui::KeycodeConverter::DomCodeToNativeKeycode(event.code()));
    bool pressed = (event.type() == ui::ET_KEY_PRESSED);
    // TODO(mukai): consolidate the definition of this modifiers_mask with other
    // similar ones in components/exo/keyboard.cc or arc_ime_service.cc.
    constexpr uint32_t modifiers_mask =
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
        ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
    // 1-bit shifts to adjust the bitpattern for the modifiers; see also
    // WaylandTextInputDelegate::SendModifiers().
    uint32_t modifiers = (event.flags() & modifiers_mask) >> 1;

    zwp_text_input_v1_send_keysym(
        text_input_, TimeTicksToMilliseconds(event.time_stamp()),
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        keysym,
        pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                : WL_KEYBOARD_KEY_STATE_RELEASED,
        modifiers);
    wl_client_flush(client());
  }

  void OnTextDirectionChanged(base::i18n::TextDirection direction) override {
    uint32_t wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_AUTO;
    switch (direction) {
      case base::i18n::RIGHT_TO_LEFT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_LTR;
        break;
      case base::i18n::LEFT_TO_RIGHT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_RTL;
        break;
      case base::i18n::UNKNOWN_DIRECTION:
        LOG(ERROR) << "Unrecognized direction: " << direction;
    }

    zwp_text_input_v1_send_text_direction(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        wayland_direction);
  }

  wl_resource* text_input_;
  wl_resource* surface_ = nullptr;

  // Owned by Seat, which is updated before calling the callbacks of this
  // class.
  const XkbTracker* const xkb_tracker_;

  // Owned by Server, which always outlives this delegate.
  SerialTracker* const serial_tracker_;
};

void text_input_activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         wl_resource* surface_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  static_cast<WaylandTextInputDelegate*>(text_input->delegate())
      ->set_surface(surface_resource);
  text_input->Activate(surface);

  // Sending modifiers.
  constexpr const char* kModifierNames[] = {
      XKB_MOD_NAME_SHIFT,
      XKB_MOD_NAME_CTRL,
      XKB_MOD_NAME_ALT,
      XKB_MOD_NAME_LOGO,
      "Mod5",
      "Mod3",
  };
  wl_array modifiers;
  wl_array_init(&modifiers);
  for (const char* modifier : kModifierNames) {
    char* p =
        static_cast<char*>(wl_array_add(&modifiers, ::strlen(modifier) + 1));
    ::strcpy(p, modifier);
  }
  zwp_text_input_v1_send_modifiers_map(resource, &modifiers);
  wl_array_release(&modifiers);
}

void text_input_deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  text_input->Deactivate();
}

void text_input_show_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->ShowVirtualKeyboardIfEnabled();
}

void text_input_hide_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->HideVirtualKeyboard();
}

void text_input_reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->Resync();
}

void text_input_set_surrounding_text(wl_client* client,
                                     wl_resource* resource,
                                     const char* text,
                                     uint32_t cursor,
                                     uint32_t anchor) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  text_input->SetSurroundingText(base::UTF8ToUTF16(text),
                                 OffsetFromUTF8Offset(text, cursor),
                                 OffsetFromUTF8Offset(text, anchor));
}

void text_input_set_content_type(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t hint,
                                 uint32_t purpose) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  ui::TextInputType type = ui::TEXT_INPUT_TYPE_TEXT;
  ui::TextInputMode mode = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning = true;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT) {
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF |
             ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF;
  }
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA)
    should_do_learning = false;
  // Unused hints: LATIN, MULTILINE.

  switch (purpose) {
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS:
      mode = ui::TEXT_INPUT_MODE_DECIMAL;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER:
      mode = ui::TEXT_INPUT_MODE_NUMERIC;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE:
      mode = ui::TEXT_INPUT_MODE_TEL;
      type = ui::TEXT_INPUT_TYPE_TELEPHONE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL:
      mode = ui::TEXT_INPUT_MODE_URL;
      type = ui::TEXT_INPUT_TYPE_URL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL:
      mode = ui::TEXT_INPUT_MODE_EMAIL;
      type = ui::TEXT_INPUT_TYPE_EMAIL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD:
      DCHECK(!should_do_learning);
      type = ui::TEXT_INPUT_TYPE_PASSWORD;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE:
      type = ui::TEXT_INPUT_TYPE_DATE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME:
      type = ui::TEXT_INPUT_TYPE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME:
      type = ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL:
      // No special type / mode are set.
      break;
  }

  text_input->SetTypeModeFlags(type, mode, flags, should_do_learning);
}

void text_input_set_cursor_rectangle(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<TextInput>(resource)->SetCaretBounds(
      gfx::Rect(x, y, width, height));
}

void text_input_set_preferred_language(wl_client* client,
                                       wl_resource* resource,
                                       const char* language) {
  // Nothing needs to be done.
}

void text_input_commit_state(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  // Nothing needs to be done.
}

void text_input_invoke_action(wl_client* client,
                              wl_resource* resource,
                              uint32_t button,
                              uint32_t index) {
  GetUserDataAs<TextInput>(resource)->Resync();
}

const struct zwp_text_input_v1_interface text_input_v1_implementation = {
    text_input_activate,
    text_input_deactivate,
    text_input_show_input_panel,
    text_input_hide_input_panel,
    text_input_reset,
    text_input_set_surrounding_text,
    text_input_set_content_type,
    text_input_set_cursor_rectangle,
    text_input_set_preferred_language,
    text_input_commit_state,
    text_input_invoke_action,
};

////////////////////////////////////////////////////////////////////////////////
// text_input_manager_v1 interface:

void text_input_manager_create_text_input(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id) {
  auto* data = GetUserDataAs<WaylandTextInputManager>(resource);

  wl_resource* text_input_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface, 1, id);

  SetImplementation(
      text_input_resource, &text_input_v1_implementation,
      std::make_unique<TextInput>(std::make_unique<WaylandTextInputDelegate>(
          text_input_resource, data->xkb_tracker, data->serial_tracker)));
}

const struct zwp_text_input_manager_v1_interface
    text_input_manager_implementation = {
        text_input_manager_create_text_input,
};

}  // namespace

void bind_text_input_manager(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_text_input_manager_v1_interface, 1, id);
  wl_resource_set_implementation(resource, &text_input_manager_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
