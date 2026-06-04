// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <algorithm>
#include <memory>

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

struct OmniboxState : public base::SupportsUserData::Data {
  explicit OmniboxState(const OmniboxEditModel::State& model_state,
                        const gfx::Range& selection);

  ~OmniboxState() override;

  const OmniboxEditModel::State model_state;
  // Views omnibox also keeps track of saved selection when focused out;
  // it's likely not needed here since the selection state in
  // WebUIReadOnlyOmnibox is disconnected from that in the actual HTML UI.
  const gfx::Range selection;
};

OmniboxState::OmniboxState(const OmniboxEditModel::State& model_state,
                           const gfx::Range& selection)
    : model_state(model_state), selection(selection) {}

OmniboxState::~OmniboxState() = default;

}  // namespace

WebUIReadOnlyOmnibox::UpdatePropagator::~UpdatePropagator() = default;

WebUIReadOnlyOmnibox::WebUIReadOnlyOmnibox(OmniboxController* controller,
                                           UpdatePropagator& update_propagator)
    : OmniboxView(controller),
      update_propagator_(update_propagator),
      selection_(gfx::Range::InvalidRange()) {}

WebUIReadOnlyOmnibox::~WebUIReadOnlyOmnibox() = default;

void WebUIReadOnlyOmnibox::SaveStateToTab(content::WebContents* tab) {
  const OmniboxEditModel::State state =
      controller()->edit_model()->GetStateForTabSwitch();
  tab->SetUserData(OmniboxTabHelper::kOmniboxStateKey,
                   std::make_unique<OmniboxState>(state, selection_));
}

void WebUIReadOnlyOmnibox::OnTabChanged(
    const content::WebContents* web_contents) {
  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  controller()->edit_model()->RestoreState(state ? &state->model_state
                                                 : nullptr);
  if (state) {
    selection_ = state->selection;
  }

  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxTabHelper::kOmniboxStateKey, nullptr);
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action) {
  switch (action->which()) {
    case toolbar_ui_api::mojom::OmniboxAction::Tag::kFocusChange:
      return OnFocusChange(*action->get_focus_change());

    case toolbar_ui_api::mojom::OmniboxAction::Tag::kTextInput:
      return OnTextInput(*action->get_text_input());

    case toolbar_ui_api::mojom::OmniboxAction::Tag::kKey:
      return OnKey(*action->get_key());
  }
}

void WebUIReadOnlyOmnibox::Update() {
  // TODO(crbug.com/474060468): Identical to OmniboxViewViews; need a sharing
  // strategy.
  if (controller()->edit_model()->ResetDisplayTexts()) {
    RevertAll();

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus.
    if (controller()->edit_model()->has_focus()) {
      SelectAll(true);
    }
  } else {
    // If the text is unchanged, we still need to re-emphasize the text, as the
    // security state may be different from before the Update.
    EmphasizeURLComponents();
  }
}

void WebUIReadOnlyOmnibox::SetTextAndSelectedRange(
    const std::u16string& text,
    const std::u16string& inline_autocompletion,
    const gfx::Range& selection) {
  text_ = text;
  inline_autocompletion_ = inline_autocompletion;

  // The JS side will likely render the inline completion using selection,
  // but conceptually we're at end of text.
  selection_ = gfx::Range(text.size());
  ResetFormatting();
}

std::u16string WebUIReadOnlyOmnibox::GetText() const {
  return text_;
}

void WebUIReadOnlyOmnibox::SetWindowTextAndCaretPos(const std::u16string& text,
                                                    size_t caret_pos,
                                                    bool update_popup,
                                                    bool notify_text_changed) {
  text_ = text;
  selection_ = gfx::Range(caret_pos);
  ResetFormatting();

  if (update_popup) {
    UpdatePopup();
  }

  if (notify_text_changed) {
    TextChanged();
  }

  SetAdditionalText(std::u16string());
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::SetCaretPos(size_t caret_pos) {
  selection_ = gfx::Range(caret_pos);

  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::SetAdditionalText(
    const std::u16string& additional_text) {
  additional_text_ = FormatOmniboxAdditionalText(additional_text);
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::EnterKeywordModeForDefaultSearchProvider() {
  controller()->edit_model()->EnterKeywordModeForDefaultSearchProvider(
      metrics::OmniboxEventProto::KEYBOARD_SHORTCUT);
  ResetBrowserVersion();
  RequestUpdateWebUI();
}

bool WebUIReadOnlyOmnibox::IsSelectAll() const {
  if (text_.empty()) {
    return false;
  }

  return selection_.GetMin() == 0 && selection_.length() == text_.length();
}

gfx::Range WebUIReadOnlyOmnibox::GetSelectionBounds() const {
  return selection_;
}

void WebUIReadOnlyOmnibox::SelectAll(bool reversed) {
  size_t length = text_.size();
  selection_ = reversed ? gfx::Range(length, 0) : gfx::Range(0, length);
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::UpdatePopup() {
  controller()->edit_model()->UpdateInput(
      /*prevent_inline_autocomplete=*/selection_.GetMin() != text_.size());
}

void WebUIReadOnlyOmnibox::RevertAll() {
  OmniboxView::RevertAll();
  if (auto* popup_closer = controller()->client()->GetOmniboxPopupCloser()) {
    popup_closer->CloseWithReason(omnibox::PopupCloseReason::kRevertAll);
  }
  ResetBrowserVersion();
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::SetFocus(bool is_user_initiated) {
  SetFocusWithTarget(
      is_user_initiated
          ? toolbar_ui_api::mojom::FocusRequestTarget::kLocationBarUserInitiated
          : toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);
}

bool WebUIReadOnlyOmnibox::AimButtonVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIReadOnlyOmnibox::ApplyCaretVisibility() {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::OnTemporaryTextMaybeChanged(
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection) {
    saved_selection_for_temporary_text_ = selection_;
  }

  // This will call RequestUpdateWebUI(), so we don't have to.
  ResetBrowserVersion();
  SetWindowTextAndCaretPos(display_text, display_text.length(),
                           /*update_popup=*/false, notify_text_changed);
}

void WebUIReadOnlyOmnibox::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& user_text,
    const std::u16string& inline_autocompletion) {
  if (user_text == text_ && inline_autocompletion == inline_autocompletion_) {
    return;
  }

  // The JS side will likely render the inline completion using selection,
  // but conceptually we're at end of text.
  gfx::Range selection(user_text.size());
  SetTextAndSelectedRange(user_text, inline_autocompletion, selection);
  SetAdditionalText(std::u16string());
  ResetFormatting();
  EmphasizeURLComponents();
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::OnInlineAutocompleteTextCleared() {
  inline_autocompletion_.clear();
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::OnRevertTemporaryText(
    const std::u16string& display_text,
    const AutocompleteMatch& match) {
  // Just restore the selection; the model has already taken care of the text.
  selection_ = saved_selection_for_temporary_text_;
  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::OnBeforePossibleChange() {
  state_before_change_ = GetState();
}

bool WebUIReadOnlyOmnibox::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  State new_state = GetState();
  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  bool something_changed = controller()->edit_model()->OnAfterPossibleChange(
      state_changes, allow_keyword_ui_change);

  // TODO(crbug.com/503784450): Might need to do unelision.
  if (something_changed &&
      (state_changes.text_differs || state_changes.keyword_differs)) {
    TextChanged();
  } else if (state_changes.selection_differs) {
    EmphasizeURLComponents();
  }

  return something_changed;
}

int WebUIReadOnlyOmnibox::GetOmniboxTextLength() const {
  return text_.size();
}

void WebUIReadOnlyOmnibox::EmphasizeURLComponents() {
  // TODO(crbug.com/474060468): remove dupe w/Views impl.
  text_is_url_ = controller()->edit_model()->CurrentTextIsURL();
  text_strike_through_.ClearAndSetInitialValue(false);

  UpdateTextStyle(text_, text_is_url_,
                  controller()->client()->GetSchemeClassifier());
}

void WebUIReadOnlyOmnibox::SetEmphasis(bool emphasize,
                                       const gfx::Range& range) {
  toolbar_ui_api::mojom::OmniboxTextColor color =
      emphasize ? toolbar_ui_api::mojom::OmniboxTextColor::kOmniboxText
                : toolbar_ui_api::mojom::OmniboxTextColor::kOmniboxTextDimmed;
  if (range.IsValid()) {
    text_colors_.ApplyValue(color, range);
  } else {
    text_colors_.ClearAndSetInitialValue(color);
  }
}

void WebUIReadOnlyOmnibox::UpdateSchemeStyle(const gfx::Range& range) {
  // TODO(crbug.com/474060468): partial dupe with OmniboxViewViews
  DCHECK(range.IsValid());
  DCHECK(!controller()->edit_model()->user_input_in_progress());

  // Do not style the scheme for non-http/https URLs. For such schemes, styling
  // could be confusing or misleading. For example, the scheme isn't meaningful
  // in about:blank URLs. Or in blob: or filesystem: URLs, which have an inner
  // origin, the URL is likely too syntax-y to be able to meaningfully draw
  // attention to any part of it.
  if (!controller()->client()->GetNavigationEntryURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (net::IsCertStatusError(controller()->client()->GetCertStatus())) {
    toolbar_ui_api::mojom::OmniboxTextColor color =
        toolbar_ui_api::mojom::OmniboxTextColor::kOmniboxText;
    if (controller()->client()->GetSecurityLevel() ==
        security_state::DANGEROUS) {
      color = toolbar_ui_api::mojom::OmniboxTextColor::
          kOmniboxSecurityChipDangerous;
    }
    text_colors_.ApplyValue(color, range);
    text_strike_through_.ApplyValue(true, range);
  }
}

toolbar_ui_api::mojom::OmniboxViewStatePtr
WebUIReadOnlyOmnibox::ComputeMojoState() const {
  auto state = toolbar_ui_api::mojom::OmniboxViewState::New();
  state->ui_version = ui_version_;
  state->browser_version = browser_version_;
  if (selection_.IsValid()) {
    state->selection = selection_;
  }
  state->inline_autocompletion = inline_autocompletion_;
  state->text_is_url = text_is_url_;
  state->additional_text = additional_text_;

  // Figure out all the breakpoints so we can go through text span-by-span.
  std::vector<size_t> breakpoints;
  for (const auto& b : text_strike_through_.breaks()) {
    breakpoints.push_back(b.first);
  }
  for (const auto& b : text_colors_.breaks()) {
    breakpoints.push_back(b.first);
  }
  // Add size for convenience.
  breakpoints.push_back(text_.size());
  std::ranges::sort(breakpoints);
  auto [rm_begin, rm_end] = std::ranges::unique(breakpoints);
  breakpoints.erase(rm_begin, rm_end);

  // Now we can just split the text into pieces w/proper formatting.
  for (size_t i = 0; i < breakpoints.size() - 1; ++i) {
    size_t begin = breakpoints[i];
    size_t end = breakpoints[i + 1];

    state->text_pieces.push_back(toolbar_ui_api::mojom::OmniboxTextPortion::New(
        base::UTF16ToUTF8(text_.substr(begin, end - begin)),
        text_strike_through_.GetBreak(begin)->second,
        text_colors_.GetBreak(begin)->second));
  }

  return state;
}

void WebUIReadOnlyOmnibox::SetFocusWithTarget(
    toolbar_ui_api::mojom::FocusRequestTarget target) {
  update_propagator_->PropagateFocusRequest(target);

  // If the user attempts to focus the omnibox, and the ctrl key is pressed, we
  // want to prevent ctrl-enter behavior until the ctrl key is released and
  // re-pressed. This occurs even if the omnibox is already focused and we
  // re-request focus (e.g. pressing ctrl-l twice).
  controller()->edit_model()->ConsumeCtrlKey();
}

void WebUIReadOnlyOmnibox::RequestUpdateWebUI() {
  update_propagator_->PropagateOmniboxUpdate(ComputeMojoState());
}

void WebUIReadOnlyOmnibox::ResetFormatting() {
  text_strike_through_.SetMax(text_.size());
  text_colors_.SetMax(text_.size());
  text_strike_through_.ClearAndSetInitialValue(false);
  text_colors_.ClearAndSetInitialValue(
      toolbar_ui_api::mojom::OmniboxTextColor::kOmniboxText);
}

void WebUIReadOnlyOmnibox::ResetBrowserVersion() {
  ++browser_version_;
  ui_version_ = 0;
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnFocusChange(
    const toolbar_ui_api::mojom::OmniboxActionFocusChange& focus_change) {
  if (focus_change.has_focus) {
    // TODO(crbug.com/500653057): Key state, though Views impl doesn't have it.
    // TODO(crbug.com/503784990): May have to call ConsumeCtrlKey() when
    //   acquiring focus, including via Ctrl-L.
    controller()->edit_model()->OnSetFocus(/*control_down=*/false);
  } else {
    controller()->edit_model()->OnWillKillFocus();
    if (auto* popup_closer = controller()->client()->GetOmniboxPopupCloser()) {
      popup_closer->CloseWithReason(omnibox::PopupCloseReason::kBlur);
    }
    controller()->edit_model()->OnKillFocus();
  }
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnTextInput(
    const toolbar_ui_api::mojom::OmniboxActionTextInput& text_input) {
  if (text_input.browser_version == browser_version_) {
    OnBeforePossibleChange();
    ui_version_ = text_input.ui_version;
    SetTextAndSelectedRange(text_input.text, text_input.inline_autocompletion,
                            gfx::Range(text_input.text.size()));
    OnAfterPossibleChange(/*allow_keyword_ui_change=*/true);
  }
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnKey(
    const toolbar_ui_api::mojom::OmniboxActionKey& key) {
  auto decoded_modifiers =
      browser_controls_api::BrowserControlsService::ToUiEventFlags(
          key.modifiers);
  if (!decoded_modifiers.has_value()) {
    return base::unexpected(std::move(decoded_modifiers).error());
  }

  ui::EventFlags event_flags = *decoded_modifiers;
  const bool shift = event_flags & ui::EF_SHIFT_DOWN;
  const bool control = event_flags & ui::EF_CONTROL_DOWN;
  const bool alt =
      (event_flags & ui::EF_ALT_DOWN) || (event_flags & ui::EF_ALTGR_DOWN);
  const bool command = event_flags & ui::EF_COMMAND_DOWN;

  ui::DomKey dom_key = LookupAndCacheDomKey(key.key);
  if (dom_key == ui::DomKey::CONTROL) {
    controller()->edit_model()->OnControlKeyChanged(key.is_key_down);
    return base::ok(std::monostate());
  }

  if (!key.is_key_down) {
    // We only care about keyup for control.
    return base::ok(std::monostate());
  }

  switch (dom_key) {
    case ui::DomKey::ENTER: {
      WindowOpenDisposition disposition =
          ComputeOpenDispositionFromModifiersAndLogToUma(shift, control, alt,
                                                         command);
      // TODO(crbug.com/503784580): Views impl has some special handling of
      //   AIM button here. We may or may not need it depending on how we
      //   implement its focus behavior.
      if (!control) {
        controller()->edit_model()->OpenCurrentSelection(base::TimeTicks::Now(),
                                                         disposition,
                                                         /*via_keyboard=*/true);
      } else {
        // Ctrl+Enter has special magic behavior where it can append www. and
        // .com if needed.
        controller()->edit_model()->OpenSelection(
            OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                  OmniboxPopupSelection::LineState::NORMAL),
            base::TimeTicks::Now(), disposition, /*via_keyboard=*/true);
      }
      break;
    }

    case ui::DomKey::ESCAPE:
      controller()->edit_model()->OnEscapeKeyPressed();
      break;

    case ui::DomKey::ARROW_UP:
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/false,
                                                    /*page=*/false);
      break;

    case ui::DomKey::ARROW_DOWN:
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/true,
                                                    /*page=*/false);
      break;

    case ui::DomKey::FromCharacter(' '):
      // This is relying on search keyword activation incrementing browser
      // version to resolve the conflict with text input with ' ' appended
      // that's incoming --- the JS side doesn't know whether the space will
      // trigger the keyboard or not.
      controller()->edit_model()->OnSpacePressed();
      break;

    case ui::DomKey::BACKSPACE:
      if (controller()->edit_model()->is_keyword_selected()) {
        controller()->edit_model()->ClearKeyword();
      }
      break;

    default:
      break;
  }
  return base::ok(std::monostate());
}

ui::DomKey WebUIReadOnlyOmnibox::LookupAndCacheDomKey(
    std::string_view key_str) {
  // ui::KeycodeConverter is quite slow for looking up by KeyEvent.Key strings,
  // (well, primarily ' '), but it's unclear for most usages it makes sense to
  // make it more sophisticated... So instead cache what we use, which is a tiny
  // number of keys, so should be quite cheap.
  if (auto it = key_code_cache_.find(key_str); it != key_code_cache_.end()) {
    return it->second;
  }
  ui::DomKey dom_key = ui::KeycodeConverter::KeyStringToDomKey(key_str);
  key_code_cache_.insert(std::pair(std::string(key_str), dom_key));
  return dom_key;
}
