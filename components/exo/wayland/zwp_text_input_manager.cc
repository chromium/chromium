// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_text_input_manager.h"

#include <sys/mman.h>
#include <text-input-extension-unstable-v1-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xkbcommon/xkbcommon.h>

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/display.h"
#include "components/exo/text_input.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/xkb_tracker.h"
#include "net/base/data_url.h"
#include "ui/base/ime/utf_offset.h"
#include "ui/base/wayland/wayland_server_input_types.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"

namespace exo {
namespace wayland {

namespace {

// The list of modifiers that this supports.
// This is consistent with X.h.
constexpr const char* kModifierNames[] = {
    XKB_MOD_NAME_SHIFT, XKB_MOD_NAME_CAPS,
    XKB_MOD_NAME_CTRL,  XKB_MOD_NAME_ALT,
    XKB_MOD_NAME_NUM,   "Mod3",
    XKB_MOD_NAME_LOGO,  "Mod5",
};
uint32_t keyCharToKeySym(char16_t keychar) {
  // TODO(b/237461655): Lacros fails to handle key presses properly when the
  // key character is not present in the keyboard layout.
  if ((keychar >= 0x20 && keychar <= 0x7e) ||
      (keychar >= 0xa0 && keychar <= 0xff)) {
    return keychar;
  }
  // The spec also requires event.GetCharacter() <= 0x10ffff but this is
  // always true due to the type of event.GetCharacter().
  if (keychar >= 0x100) {
    return keychar + 0x01000000;
  }
  // keysym 0 is used for unidentified events
  return 0;
}

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

  void set_extended_text_input(wl_resource* extended_text_input) {
    extended_text_input_ = extended_text_input;
  }

  bool has_extended_text_input() const { return extended_text_input_; }

  base::WeakPtr<WaylandTextInputDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  wl_resource* resource() { return text_input_; }

  ui::TextInputClient::FocusReason pending_focus_reason() const {
    return pending_focus_reason_;
  }

  void set_pending_focus_reason(ui::TextInputClient::FocusReason reason) {
    pending_focus_reason_ = reason;
  }

  bool pending_surrounding_text_supported() const {
    return pending_surrounding_text_supported_;
  }

  void set_pending_surrounding_text_supported(bool is_supported) {
    pending_surrounding_text_supported_ = is_supported;
  }

  void SetPendingGrammarFragment(
      const std::optional<ui::GrammarFragment>& grammar_fragment) {
    pending_grammar_fragment_ = grammar_fragment;
  }

  std::optional<ui::GrammarFragment> TakeGrammarFragment() {
    auto result = pending_grammar_fragment_;
    pending_grammar_fragment_.reset();
    return result;
  }

  void SetPendingAutocorrectInfo(const ui::AutocorrectInfo& autocorrect_info) {
    pending_autocorrect_info_ = autocorrect_info;
  }

  std::optional<ui::AutocorrectInfo> TakeAutocorrectInfo() {
    auto result = pending_autocorrect_info_;
    pending_autocorrect_info_.reset();
    return result;
  }

  void SetSurroundingTextOffsetUtf16(uint32_t offset) {
    pending_surrounding_text_offset_utf16_ = offset;
  }

  std::optional<uint32_t> TakeSurroundingTextOffsetUtf16() {
    auto result = pending_surrounding_text_offset_utf16_;
    pending_surrounding_text_offset_utf16_.reset();
    return result;
  }

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
    // The detailed spec of |state| is implementation dependent.
    // So, now we use the lowest bit to indicate whether keyboard is visible.
    // This behavior is consistent with ozone/wayland to support Lacros.
    zwp_text_input_v1_send_input_panel_state(text_input_,
                                             static_cast<uint32_t>(is_visible));
    wl_client_flush(client());
  }

  void OnVirtualKeyboardOccludedBoundsChanged(
      const gfx::Rect& screen_bounds) override {
    if (!extended_text_input_)
      return;

    if (wl_resource_get_version(extended_text_input_) >=
        ZCR_EXTENDED_TEXT_INPUT_V1_SET_VIRTUAL_KEYBOARD_OCCLUDED_BOUNDS_SINCE_VERSION) {
      zcr_extended_text_input_v1_send_set_virtual_keyboard_occluded_bounds(
          extended_text_input_, screen_bounds.x(), screen_bounds.y(),
          screen_bounds.width(), screen_bounds.height());
      wl_client_flush(client());
    }
  }

  bool SupportsFinalizeVirtualKeyboardChanges() override {
    return extended_text_input_ &&
           wl_resource_get_version(extended_text_input_) >=
               ZCR_EXTENDED_TEXT_INPUT_V1_FINALIZE_VIRTUAL_KEYBOARD_CHANGES_SINCE_VERSION;
  }

  void SetCompositionText(const ui::CompositionText& composition) override {
    SendPreeditStyle(composition.text, composition.ime_text_spans);

    std::vector<size_t> offsets = {composition.selection.start()};
    const std::string utf8 =
        base::UTF16ToUTF8AndAdjustOffsets(composition.text, &offsets);

    if (offsets[0] != std::string::npos)
      zwp_text_input_v1_send_preedit_cursor(text_input_, offsets[0]);

    zwp_text_input_v1_send_preedit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        utf8.c_str(), utf8.c_str());

    wl_client_flush(client());
  }

  void Commit(std::u16string_view text) override {
    zwp_text_input_v1_send_commit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        base::UTF16ToUTF8(text).c_str());
    wl_client_flush(client());
  }

  void SetCursor(std::u16string_view surrounding_text,
                 const gfx::Range& selection) override {
    std::vector<size_t> offsets{selection.start(), selection.end()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    zwp_text_input_v1_send_cursor_position(text_input_,
                                           static_cast<uint32_t>(offsets[1]),
                                           static_cast<uint32_t>(offsets[0]));
  }

  void DeleteSurroundingText(std::u16string_view surrounding_text,
                             const gfx::Range& range) override {
    std::vector<size_t> offsets{range.GetMin(), range.GetMax()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    // Currently, the arguments are conflicting with spec.
    // However, the only client, Lacros, also interprets wrongly in the same
    // way so just fixing here could cause visible regression.
    // TODO(crbug.com/40189286): Fix the behavior with versioning.
    zwp_text_input_v1_send_delete_surrounding_text(
        text_input_, static_cast<uint32_t>(offsets[0]),
        static_cast<uint32_t>(offsets[1] - offsets[0]));
    wl_client_flush(client());
  }

  void SendKey(const ui::KeyEvent& event) override {
    uint32_t keysym =
        event.code() != ui::DomCode::NONE
            ? xkb_tracker_->GetKeysym(
                  ui::KeycodeConverter::DomCodeToNativeKeycode(event.code()))
            : 0;
    // Some artificial key events (e.g. from virtual keyboard) do not set code,
    // so must be handled separately.
    // https://www.x.org/releases/X11R7.6/doc/xproto/x11protocol.html#keysym_encoding
    // suggests that we can just directly map some parts of unicode.
    if (keysym == 0) {
      keysym = keyCharToKeySym(event.GetCharacter());
    }

    if (keysym == 0) {
      VLOG(0) << "Unable to find keysym for: " << event.ToString();
    }

    bool pressed = (event.type() == ui::EventType::kKeyPressed);
    zwp_text_input_v1_send_keysym(
        text_input_, TimeTicksToMilliseconds(event.time_stamp()),
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        keysym,
        pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                : WL_KEYBOARD_KEY_STATE_RELEASED,
        modifier_converter_.MaskFromUiFlags(event.flags()));
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

  void SetCompositionFromExistingText(
      std::u16string_view surrounding_text,
      const gfx::Range& cursor,
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override {
    if (!extended_text_input_)
      return;

    uint32_t begin = range.GetMin();
    uint32_t end = range.GetMax();
    SendPreeditStyle(surrounding_text.substr(begin, range.length()),
                     ui_ime_text_spans);

    std::vector<size_t> offsets = {begin, end, cursor.end()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    int32_t index =
        static_cast<int32_t>(offsets[0]) - static_cast<int32_t>(offsets[2]);
    uint32_t length = static_cast<uint32_t>(offsets[1] - offsets[0]);
    zcr_extended_text_input_v1_send_set_preedit_region(extended_text_input_,
                                                       index, length);
    wl_client_flush(client());
  }

  void ClearGrammarFragments(std::u16string_view surrounding_text,
                             const gfx::Range& range) override {
    if (!extended_text_input_)
      return;

    if (wl_resource_get_version(extended_text_input_) >=
        ZCR_EXTENDED_TEXT_INPUT_V1_CLEAR_GRAMMAR_FRAGMENTS_SINCE_VERSION) {
      std::vector<size_t> offsets = {range.start(), range.end()};
      base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
      zcr_extended_text_input_v1_send_clear_grammar_fragments(
          extended_text_input_, static_cast<uint32_t>(offsets[0]),
          static_cast<uint32_t>(offsets[1]));
      wl_client_flush(client());
    }
  }

  void AddGrammarFragment(std::u16string_view surrounding_text,
                          const ui::GrammarFragment& fragment) override {
    if (!extended_text_input_)
      return;

    if (wl_resource_get_version(extended_text_input_) >=
        ZCR_EXTENDED_TEXT_INPUT_V1_ADD_GRAMMAR_FRAGMENT_SINCE_VERSION) {
      std::vector<size_t> offsets = {fragment.range.start(),
                                     fragment.range.end()};
      base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
      zcr_extended_text_input_v1_send_add_grammar_fragment(
          extended_text_input_, static_cast<uint32_t>(offsets[0]),
          static_cast<uint32_t>(offsets[1]), fragment.suggestion.c_str());
      wl_client_flush(client());
    }
  }

  void SetAutocorrectRange(std::u16string_view surrounding_text,
                           const gfx::Range& range) override {
    if (!extended_text_input_) {
      return;
    }

    if (wl_resource_get_version(extended_text_input_) <
        ZCR_EXTENDED_TEXT_INPUT_V1_SET_AUTOCORRECT_RANGE_SINCE_VERSION) {
      return;
    }

    std::vector<size_t> offsets{range.GetMin(), range.GetMax()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    zcr_extended_text_input_v1_send_set_autocorrect_range(
        extended_text_input_, offsets[0], offsets[1]);
    wl_client_flush(client());
  }

  bool HasImageInsertSupport() override {
    if (!extended_text_input_) {
      return false;
    }

    return wl_resource_get_version(extended_text_input_) >=
           ZCR_EXTENDED_TEXT_INPUT_V1_INSERT_IMAGE_SINCE_VERSION;
  }

  void InsertImage(const GURL& src) override {
    if (!extended_text_input_) {
      return;
    }

    // Due to the limit of wayland protocol, we should check the size of
    // payload here.
    static constexpr size_t kSizeLimit = 4000;
    const size_t src_size = src.spec().size();
    if (src_size > kSizeLimit) {
      if (wl_resource_get_version(extended_text_input_) <
          ZCR_EXTENDED_TEXT_INPUT_V1_INSERT_IMAGE_WITH_LARGE_URL_SINCE_VERSION) {
        LOG(ERROR) << "Inserting image with large URL is not supported";
        return;
      }

      std::string mime_type, charset, raw_data;
      if (!net::DataURL::Parse(src, &mime_type, &charset, &raw_data)) {
        LOG(ERROR) << "Failed to parse data url";
        return;
      }

      base::ScopedFD memfd(memfd_create("inserting_image", MFD_CLOEXEC));
      if (!memfd.get()) {
        PLOG(ERROR) << "Failed to create memfd";
        return;
      }

      if (!base::WriteFileDescriptor(memfd.get(), raw_data)) {
        LOG(ERROR) << "Failed to write into memfd";
        return;
      }
      if (lseek(memfd.get(), 0, SEEK_SET) != 0) {
        LOG(ERROR) << "Failed to reset file descriptor";
        return;
      }
      zcr_extended_text_input_v1_send_insert_image_with_large_url(
          extended_text_input_, mime_type.c_str(), charset.c_str(), memfd.get(),
          raw_data.size());
      wl_client_flush(client());
      return;
    }

    if (wl_resource_get_version(extended_text_input_) >=
        ZCR_EXTENDED_TEXT_INPUT_V1_INSERT_IMAGE_SINCE_VERSION) {
      zcr_extended_text_input_v1_send_insert_image(extended_text_input_,
                                                   src.spec().c_str());
      wl_client_flush(client());
    }
  }

  void SendPreeditStyle(std::u16string_view text,
                        const std::vector<ui::ImeTextSpan>& spans) {
    if (spans.empty())
      return;

    // Convert all offsets from UTF16 to UTF8.
    std::vector<size_t> offsets;
    offsets.reserve(spans.size() * 2);
    for (const auto& span : spans) {
      auto minmax = std::minmax(span.start_offset, span.end_offset);
      offsets.push_back(minmax.first);
      offsets.push_back(minmax.second);
    }
    base::UTF16ToUTF8AndAdjustOffsets(text, &offsets);

    for (size_t i = 0; i < spans.size(); ++i) {
      if (offsets[i * 2] == std::string::npos ||
          offsets[i * 2 + 1] == std::string::npos) {
        // Invalid span is specified.
        continue;
      }
      const auto& span = spans[i];
      const uint32_t begin = offsets[i * 2];
      const uint32_t end = offsets[i * 2 + 1];

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
        case ui::ImeTextSpan::Type::kGrammarSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT;
          break;
      }
      zwp_text_input_v1_send_preedit_styling(text_input_, begin, end - begin,
                                             style);
    }
  }

  bool ConfirmComposition(bool keep_selection) override {
    if (!extended_text_input_) {
      return false;
    }

    if (wl_resource_get_version(extended_text_input_) <
        ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SINCE_VERSION) {
      return false;
    }

    zcr_extended_text_input_v1_send_confirm_preedit(
        extended_text_input_,
        keep_selection
            ? ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_UNCHANGED
            : ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_AFTER_PREEDIT);
    wl_client_flush(client());
    return true;
  }

  bool SupportsConfirmPreedit() override {
    // Note: until this is supported by crostini, crostini won't be able to add
    // the new extension api.
    return extended_text_input_ &&
           wl_resource_get_version(extended_text_input_) >=
               ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SINCE_VERSION;
  }

  raw_ptr<wl_resource, DanglingUntriaged> text_input_;
  raw_ptr<wl_resource, DanglingUntriaged> extended_text_input_ = nullptr;
  raw_ptr<wl_resource, DanglingUntriaged> surface_ = nullptr;

  // Owned by Seat, which is updated before calling the callbacks of this
  // class.
  const raw_ptr<const XkbTracker> xkb_tracker_;

  // Owned by Server, which always outlives this delegate.
  const raw_ptr<SerialTracker> serial_tracker_;
  ui::XkbModifierConverter modifier_converter_{
      std::vector<std::string>(std::begin(kModifierNames),
                               std::end(kModifierNames))};

  // Pending focus reason.
  ui::TextInputClient::FocusReason pending_focus_reason_ =
      ui::TextInputClient::FOCUS_REASON_OTHER;

  // Pending surrounding text supported flag.
  bool pending_surrounding_text_supported_ = true;
  std::optional<ui::GrammarFragment> pending_grammar_fragment_;
  std::optional<ui::AutocorrectInfo> pending_autocorrect_info_;
  std::optional<std::uint32_t> pending_surrounding_text_offset_utf16_;

  base::WeakPtrFactory<WaylandTextInputDelegate> weak_factory_{this};
};

// Holds WeakPtr to WaylandTextInputDelegate, and the lifetime of this class's
// instance is tied to zcr_extended_text_input connection.
// If text_input connection is destroyed earlier than extended_text_input,
// then delegate_ will return nullptr automatically.
class WaylandExtendedTextInput {
 public:
  explicit WaylandExtendedTextInput(
      base::WeakPtr<WaylandTextInputDelegate> delegate)
      : delegate_(delegate) {}
  WaylandExtendedTextInput(const WaylandExtendedTextInput&) = delete;
  WaylandExtendedTextInput& operator=(const WaylandExtendedTextInput&) = delete;
  ~WaylandExtendedTextInput() {
    if (delegate_)
      delegate_->set_extended_text_input(nullptr);
  }

  WaylandTextInputDelegate* delegate() { return delegate_.get(); }

 private:
  base::WeakPtr<WaylandTextInputDelegate> delegate_;
};

void SetSurroundingTextImpl(TextInput* text_input,
                            WaylandTextInputDelegate* delegate,
                            std::string_view text,
                            uint32_t cursor,
                            uint32_t anchor) {
  uint32_t offset_utf16 =
      delegate->TakeSurroundingTextOffsetUtf16().value_or(0u);
  auto grammar_fragment = delegate->TakeGrammarFragment();
  auto autocorrect_info = delegate->TakeAutocorrectInfo();

  // TODO(crbug.com/40189286): Selection range should keep cursor/anchor
  // relationship.
  auto minmax = std::minmax(cursor, anchor);
  std::vector<size_t> offsets{minmax.first, minmax.second};
  if (grammar_fragment.has_value()) {
    offsets.push_back(grammar_fragment->range.start());
    offsets.push_back(grammar_fragment->range.end());
  }
  if (autocorrect_info.has_value()) {
    offsets.push_back(autocorrect_info->range.start());
    offsets.push_back(autocorrect_info->range.end());
  }

  std::u16string u16_text = base::UTF8ToUTF16AndAdjustOffsets(text, &offsets);
  if (offsets[0] == std::u16string::npos ||
      offsets[1] == std::u16string::npos) {
    return;
  }

  if (grammar_fragment.has_value()) {
    grammar_fragment->range =
        gfx::Range(offsets[2] + offset_utf16, offsets[3] + offset_utf16);
  }

  // Original implementation did not convert the range. Guard this by the
  // feature flag to be reverted to old behavior just in case for transition
  // period.
  if (autocorrect_info.has_value()) {
    size_t index = grammar_fragment.has_value() ? 4u : 2u;
    autocorrect_info->range = gfx::Range(offsets[index] + offset_utf16,
                                         offsets[index + 1] + offset_utf16);
  }

  text_input->SetSurroundingText(
      u16_text, offset_utf16,
      gfx::Range(offsets[0] + offset_utf16, offsets[1] + offset_utf16),
      grammar_fragment, autocorrect_info);
}

void text_input_activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat_resource,
                         wl_resource* surface_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  Seat* seat = GetUserDataAs<WaylandSeat>(seat_resource)->seat;
  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  delegate->set_surface(surface_resource);
  auto focus_reason = delegate->pending_focus_reason();
  delegate->set_pending_focus_reason(ui::TextInputClient::FOCUS_REASON_OTHER);
  text_input->Activate(seat, surface, focus_reason);

  // Sending modifiers.
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
  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  delegate->set_pending_focus_reason(ui::TextInputClient::FOCUS_REASON_OTHER);
  text_input->Deactivate();
}

void text_input_show_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->ShowVirtualKeyboardIfEnabled();
}

void text_input_hide_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->HideVirtualKeyboard();
}

void text_input_reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->Reset();
}

void text_input_set_surrounding_text(wl_client* client,
                                     wl_resource* resource,
                                     const char* text,
                                     uint32_t cursor,
                                     uint32_t anchor) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  SetSurroundingTextImpl(text_input, delegate, text, cursor, anchor);
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
      should_do_learning = false;
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

  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  bool surrounding_text_supported =
    delegate->pending_surrounding_text_supported();
  delegate->set_pending_surrounding_text_supported(/*is_supported = */ true);

  text_input->SetTypeModeFlags(type, mode, flags, should_do_learning,
                               /* can_compose_inline = */ true,
                               surrounding_text_supported);
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

constexpr struct zwp_text_input_v1_interface text_input_v1_implementation = {
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

constexpr struct zwp_text_input_manager_v1_interface
    text_input_manager_implementation = {
        text_input_manager_create_text_input,
};

////////////////////////////////////////////////////////////////////////////////
// extended_text_input_v1 interface:

void extended_text_input_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_text_input_set_input_type(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t input_type,
                                        uint32_t input_mode,
                                        uint32_t input_flags,
                                        uint32_t learning_mode,
                                        uint32_t inline_composition_support) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate)
    return;

  // If unknown value is passed, fall back to the default one.
  auto ui_type =
      ui::wayland::ConvertToTextInputType(
          static_cast<zcr_extended_text_input_v1_input_type>(input_type))
          .value_or(ui::TEXT_INPUT_TYPE_TEXT);
  auto ui_mode =
      ui::wayland::ConvertToTextInputMode(
          static_cast<zcr_extended_text_input_v1_input_mode>(input_mode))
          .value_or(ui::TEXT_INPUT_MODE_DEFAULT);
  // Ignore unknown flags.
  auto ui_flags = ui::wayland::ConvertToTextInputFlags(input_flags).first;
  bool should_do_learning =
      learning_mode == ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED;
  bool can_compose_inline =
      inline_composition_support ==
      ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_SUPPORTED;

  bool surrounding_text_supported =
    delegate->pending_surrounding_text_supported();
  delegate->set_pending_surrounding_text_supported(/*is_supported = */ true);

  auto* text_input = GetUserDataAs<TextInput>(delegate->resource());
  text_input->SetTypeModeFlags(ui_type, ui_mode, ui_flags, should_do_learning,
                               can_compose_inline, surrounding_text_supported);
}

void extended_text_input_deprecated_set_input_type(wl_client* client,
                                                   wl_resource* resource,
                                                   uint32_t input_type,
                                                   uint32_t input_mode,
                                                   uint32_t input_flags,
                                                   uint32_t learning_mode) {
  // TODO(crbug.com/40258785) This deprecated method signature is preserved to
  // maintain backwards compatibility with older client versions. Once both Exo
  // and Lacros have stabilized on the new API, delete this implementation or
  // otherwise make it impossible to call.
  extended_text_input_set_input_type(client, resource, input_type, input_mode,
                                     input_flags, learning_mode,
                                     /*inline_composition_support=*/true);
}

void extended_text_input_set_grammar_fragment_at_cursor(
    wl_client* client,
    wl_resource* resource,
    uint32_t start,
    uint32_t end,
    const char* suggestion) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }

  delegate->SetPendingGrammarFragment(
      start == end ? std::nullopt
                   : std::make_optional(ui::GrammarFragment(
                         gfx::Range(start, end), suggestion)));
}

void extended_text_input_set_autocorrect_info(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t start,
                                              uint32_t end,
                                              uint32_t x,
                                              uint32_t y,
                                              uint32_t width,
                                              uint32_t height) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }

  delegate->SetPendingAutocorrectInfo(ui::AutocorrectInfo{
      gfx::Range(start, end),
      gfx::Rect(x, y, width, height),
  });
}

void extended_text_input_finalize_virtual_keyboard_changes(
    wl_client* client,
    wl_resource* resource) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate)
    return;

  auto* text_input = GetUserDataAs<TextInput>(delegate->resource());
  text_input->FinalizeVirtualKeyboardChanges();
}

void extended_text_input_set_focus_reason(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t reason) {
  ui::TextInputClient::FocusReason focus_reason;
  switch (reason) {
    case ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_NONE:
      focus_reason = ui::TextInputClient::FOCUS_REASON_NONE;
      break;
    case ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_MOUSE:
      focus_reason = ui::TextInputClient::FOCUS_REASON_MOUSE;
      break;
    case ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_TOUCH:
      focus_reason = ui::TextInputClient::FOCUS_REASON_TOUCH;
      break;
    case ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_PEN:
      focus_reason = ui::TextInputClient::FOCUS_REASON_PEN;
      break;
    case ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_OTHER:
      focus_reason = ui::TextInputClient::FOCUS_REASON_OTHER;
      break;
    default:
      LOG(ERROR) << "Unknown focus reason: " << reason;
      return;
  }

  // Keep tracking in WaylandExtendedTextInput. This will be passed to
  // TextInput::Activate.
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }
  delegate->set_pending_focus_reason(focus_reason);
}

void extended_text_input_set_surrounding_text_support(wl_client* client,
                                                      wl_resource* resource,
                                                      uint32_t support) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }

  switch (support) {
    case ZCR_EXTENDED_TEXT_INPUT_V1_SURROUNDING_TEXT_SUPPORT_SUPPORTED:
      delegate->set_pending_surrounding_text_supported(/*is_supported=*/true);
      return;
    case ZCR_EXTENDED_TEXT_INPUT_V1_SURROUNDING_TEXT_SUPPORT_UNSUPPORTED:
      delegate->set_pending_surrounding_text_supported(/*is_supported=*/false);
      return;
    default:
      LOG(ERROR) << "Unknown surrounding_text_support: " << support;
      return;
  }
}

void extended_text_input_set_surrounding_text_offset_utf16(
    wl_client* client,
    wl_resource* resource,
    uint32_t offset_utf16) {
  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }

  delegate->SetSurroundingTextOffsetUtf16(offset_utf16);
}

void extended_text_input_set_large_surrounding_text(wl_client* client,
                                                    wl_resource* resource,
                                                    int32_t raw_fd,
                                                    uint32_t size,
                                                    uint32_t cursor,
                                                    uint32_t anchor) {
  std::string text;
  {
    text.resize(size);
    base::ScopedFD fd(raw_fd);
    if (!base::ReadFromFD(fd.get(), text)) {
      PLOG(ERROR) << "Failed to read file descriptor for surrounding text";
      return;
    }
  }

  auto* delegate =
      GetUserDataAs<WaylandExtendedTextInput>(resource)->delegate();
  if (!delegate) {
    return;
  }
  auto* text_input = GetUserDataAs<TextInput>(delegate->resource());
  if (!text_input) {
    return;
  }
  SetSurroundingTextImpl(text_input, delegate, text, cursor, anchor);
}

constexpr struct zcr_extended_text_input_v1_interface
    extended_text_input_implementation = {
        extended_text_input_destroy,
        extended_text_input_deprecated_set_input_type,
        extended_text_input_set_grammar_fragment_at_cursor,
        extended_text_input_set_autocorrect_info,
        extended_text_input_finalize_virtual_keyboard_changes,
        extended_text_input_set_focus_reason,
        extended_text_input_set_input_type,
        extended_text_input_set_surrounding_text_support,
        extended_text_input_set_surrounding_text_offset_utf16,
        extended_text_input_set_large_surrounding_text,
};

////////////////////////////////////////////////////////////////////////////////
// text_input_extension_v1 interface:

void text_input_extension_get_extended_text_input(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* text_input_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(text_input_resource);
  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  if (delegate->has_extended_text_input()) {
    wl_resource_post_error(
        resource, ZCR_TEXT_INPUT_EXTENSION_V1_ERROR_EXTENDED_TEXT_INPUT_EXISTS,
        "text_input has already been associated with a extended_text_input "
        "object");
    return;
  }

  uint32_t version = wl_resource_get_version(resource);
  wl_resource* extended_text_input_resource = wl_resource_create(
      client, &zcr_extended_text_input_v1_interface, version, id);

  delegate->set_extended_text_input(extended_text_input_resource);

  SetImplementation(
      extended_text_input_resource, &extended_text_input_implementation,
      std::make_unique<WaylandExtendedTextInput>(delegate->GetWeakPtr()));
}

constexpr struct zcr_text_input_extension_v1_interface
    text_input_extension_implementation = {
        text_input_extension_get_extended_text_input};

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

void bind_text_input_extension(wl_client* client,
                               void* data,
                               uint32_t version,
                               uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_text_input_extension_v1_interface, version, id);
  wl_resource_set_implementation(resource, &text_input_extension_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
