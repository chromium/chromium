// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <algorithm>
#include <memory>

#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_placeholder_util.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "components/omnibox/browser/searchbox_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/touch_selection/touch_editing_controller.h"

WebUIReadOnlyOmnibox::UpdatePropagator::~UpdatePropagator() = default;

WebUIReadOnlyOmnibox::WebUIReadOnlyOmnibox(
    LocationBar* location_bar,
    WebUIToolbarControlDelegate* toolbar_delegate,
    OmniboxController* controller,
    UpdatePropagator& update_propagator)
    : OmniboxView(controller),
      OmniboxContextMenuMixin<ui::SimpleMenuModel::Delegate>(location_bar,
                                                             controller),
      location_bar_(location_bar),
      toolbar_delegate_(toolbar_delegate),
      update_propagator_(update_propagator),
      selection_(gfx::Range::InvalidRange()) {
  // Refresh UI if the user toggles the 'show full URLs' setting.
  if (location_bar_ && location_bar_->GetProfile()) {
    pref_change_registrar_.Init(location_bar_->GetProfile()->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kPreventUrlElisionsInOmnibox,
        base::BindRepeating(&WebUIReadOnlyOmnibox::Update,
                            base::Unretained(this)));
  }

  scoped_template_url_service_observation_.Observe(
      controller->client()->GetTemplateURLService());
}

WebUIReadOnlyOmnibox::~WebUIReadOnlyOmnibox() = default;

void WebUIReadOnlyOmnibox::SaveStateToTab(content::WebContents* tab) {
  const OmniboxEditModel::State state =
      controller()->edit_model()->GetStateForTabSwitch();
  // We don't need the `saved_selection_for_focus_change` mechanism because
  // <input> won't actually lose it on blur; though it turns out we have
  // unrelated trouble with the issue that requires us to lose it sometimes
  // anyway.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          state, selection_,
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange()));
}

void WebUIReadOnlyOmnibox::OnTabChanged(content::WebContents* web_contents) {
  // Observe the WebContents for title changes and navigations for updating the
  // placeholder text.
  Observe(web_contents);

  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  controller()->edit_model()->RestoreState(state ? &state->model_state
                                                 : nullptr);
  if (state) {
    if (state->model_state.user_input_in_progress &&
        state->model_state.user_text.empty() &&
        state->model_state.keyword.empty()) {
      // See comment in OmniboxEditModel::GetStateForTabSwitch() for details on
      // this.
      SelectAll(true);
    } else {
      selection_ = state->selection;
    }
  }

  if (state && state->model_state.focus_state == OMNIBOX_FOCUS_VISIBLE) {
    SetFocus(/*is_user_initiated=*/false);
  } else if (has_focus_) {
    OnBlur();
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

    case toolbar_ui_api::mojom::OmniboxAction::Tag::kPointer:
      return OnPointer(*action->get_pointer());

    case toolbar_ui_api::mojom::OmniboxAction::Tag::kDropText:
      return OnDropText(*action->get_drop_text());

    case toolbar_ui_api::mojom::OmniboxAction::Tag::kDropFile:
      return OnDropFile(*action->get_drop_file());
  }
}

void WebUIReadOnlyOmnibox::HandleContextMenu(
    views::Widget* widget,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    const content::ContextMenuParams& menu_params) {
  menu_params_ = menu_params;
  PrepareToShowContextMenu(base::BindOnce(
      &WebUIReadOnlyOmnibox::OnContextMenuReady, weak_ptr_factory_.GetWeakPtr(),
      widget, point, source_type));
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
    const gfx::Range& selection,
    bool keep_additional_text) {
  text_ = text;
  inline_autocompletion_ = inline_autocompletion;
  selection_ = selection;
  ResetFormatting();
  if (!keep_additional_text) {
    additional_text_.clear();
  }
}

void WebUIReadOnlyOmnibox::ClearAccessibilityLabel() {
  friendly_accessible_label_.clear();
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

void WebUIReadOnlyOmnibox::SetSelectionBounds(gfx::Range selection) {
  selection_ = selection;
  RequestUpdateWebUI();
}

bool WebUIReadOnlyOmnibox::HasSelection() const {
  return true;  // <input>s always have a selection.
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

void WebUIReadOnlyOmnibox::ApplyFocusRingToAimButton(bool focus_aim) {
  aim_page_action_icon_has_fake_focus_ = focus_aim;
  update_propagator_->PropagateApplyFocusRingToAimButton(focus_aim);
}

bool WebUIReadOnlyOmnibox::AimButtonVisible() const {
  return location_bar_ &&
         omnibox::AiModePageActionController::From(location_bar_->GetBrowser())
             ->IsVisible();
}

void WebUIReadOnlyOmnibox::ApplyCaretVisibility() {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::SetAccessibilityLabel(
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool notify_text_changed) {
  // We can ignore the prefix length, since we set the suggestion separately.
  int ignored_suggestion_text_prefix_length;

  friendly_accessible_label_ = ComputeFriendlySuggestionTextForAccessibility(
      display_text, match, ignored_suggestion_text_prefix_length);

  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::OnTemporaryTextMaybeChanged(
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection) {
    saved_selection_for_temporary_text_ = selection_;
  }

  SetAccessibilityLabel(display_text, match, false);

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
  SetTextAndSelectedRange(user_text, inline_autocompletion, selection,
                          /*keep_additional_text=*/false);
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
  SetAccessibilityLabel(display_text, match, true);
  // SetAccessibilityLabel already called RequestUpdateWebUI()
}

void WebUIReadOnlyOmnibox::OnBeforePossibleChange() {
  state_before_change_ = GetState();

  // User is editing or traversing the text, as opposed to moving
  // through suggestions. Clear the accessibility label
  // so that the screen reader reports the raw text in the field.
  // This will be sent during OnAfterPossibleChange.
  ClearAccessibilityLabel();
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

void WebUIReadOnlyOmnibox::OnKeywordPlaceholderTextChange() {
  RequestUpdateWebUI();
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

void WebUIReadOnlyOmnibox::ExecuteCommand(int command_id, int event_flags) {
  if (!HandleExecuteCommand(command_id, event_flags)) {
    if (!toolbar_delegate_) {
      return;
    }
    HandleExecuteTextEditingCommandOnWebContents(
        toolbar_delegate_->GetWebContents(), command_id, event_flags);
  }
}

bool WebUIReadOnlyOmnibox::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return HandleGetAcceleratorForCommandId(command_id, accelerator);
}

bool WebUIReadOnlyOmnibox::IsContextMenuForReadOnlyOmnibox() const {
  // TODO(http://crbug.com/470042732): Once WebUILocationBar can be used for
  // popups, this will need to return true for those.
  return false;
}

const gfx::FontList& WebUIReadOnlyOmnibox::FontListForContextMenu() const {
  const auto& typography_provider = views::TypographyProvider::Get();
  return typography_provider.GetFont(CONTEXT_OMNIBOX_PRIMARY,
                                     views::style::STYLE_PRIMARY);
}

bool WebUIReadOnlyOmnibox::IsContextMenuTextEditingCommandEnabled(
    int command_id) const {
  return HandleIsContextMenuTextEditingCommandEnabled(command_id, menu_params_);
}

views::Widget* WebUIReadOnlyOmnibox::GetWidgetForTextServices() {
  return toolbar_delegate_->GetView()->GetWidget();
}

toolbar_ui_api::mojom::OmniboxViewStatePtr
WebUIReadOnlyOmnibox::ComputeMojoState() {
  auto state = toolbar_ui_api::mojom::OmniboxViewState::New();
  state->ui_version = ui_version_;
  state->browser_version = browser_version_;
  if (selection_.IsValid()) {
    state->selection = selection_;
  }
  state->formatted_full_url = controller()->client()->GetFormattedFullURL();
  state->inline_autocompletion = inline_autocompletion_;
  state->text_is_url = text_is_url_;
  state->additional_text = additional_text_;
  state->a11y_friendly_suggestion_text = friendly_accessible_label_;
  state->user_input_in_progress =
      controller()->edit_model()->user_input_in_progress();

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
        text_.substr(begin, end - begin),
        text_strike_through_.GetBreak(begin)->second,
        text_colors_.GetBreak(begin)->second));
  }

  std::u16string placeholder_text;
  // TODO(crbug.com/507045398): Pay attention to this.
  std::optional<std::u16string> maybe_a11y_placeholder;

  omnibox::ComputePlaceholderText(location_bar_, placeholder_text,
                                  maybe_a11y_placeholder);
  if (has_focus_ && !aim_hint_currently_shown_ &&
      omnibox::IsAimPlaceholderText(location_bar_, placeholder_text)) {
    omnibox::RecordAimHintImpression(location_bar_);
    aim_hint_currently_shown_ = true;
  }

  if (!placeholder_text.empty() &&
      omnibox::ShouldShowPlaceholderText(
          location_bar_,
          /*in_popup_state_transition=*/
          location_bar_->in_popup_state_transition(),
          /*aim_button_visible=*/AimButtonVisible(),
          /*aim_hint_currently_shown=*/aim_hint_currently_shown_)) {
    state->placeholder = toolbar_ui_api::mojom::OmniboxTextPortion::New(
        placeholder_text,
        /*strikethrough=*/false,
        omnibox::ShouldUseDimPlaceholderColor(location_bar_)
            ? toolbar_ui_api::mojom::OmniboxTextColor::
                  kOmniboxForegroundDisabled
            : toolbar_ui_api::mojom::OmniboxTextColor::kOmniboxText);
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

void WebUIReadOnlyOmnibox::OnBlur() {
  if (!has_focus_) {
    return;
  }
  has_focus_ = false;
  aim_hint_currently_shown_ = false;
  controller()->edit_model()->OnWillKillFocus();
  if (auto* popup_closer = controller()->client()->GetOmniboxPopupCloser()) {
    popup_closer->CloseWithReason(omnibox::PopupCloseReason::kBlur);
  }
  controller()->edit_model()->OnKillFocus();
  ClearAccessibilityLabel();
  RequestUpdateWebUI();
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnFocusChange(
    const toolbar_ui_api::mojom::OmniboxActionFocusChange& focus_change) {
  if (focus_change.has_focus) {
    has_focus_ = true;
    selection_ = focus_change.selection;
    // TODO(crbug.com/500653057): Key state, though Views impl doesn't have it.
    controller()->edit_model()->OnSetFocus(/*control_down=*/false);

    if (focus_change.request_clear_keyword) {
      controller()->edit_model()->ClearKeyword();
    }
    if (focus_change.start_zero_suggest) {
      controller()->edit_model()->StartZeroSuggestRequest();
    }
    if (focus_change.activate_default_search) {
      EnterKeywordModeForDefaultSearchProvider();
    }
    RequestUpdateWebUI();
  } else {
    OnBlur();
  }
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnTextInput(
    const toolbar_ui_api::mojom::OmniboxActionTextInput& text_input) {
  if (text_input.browser_version != browser_version_) {
    return base::ok(std::monostate());
  }

  ui_version_ = text_input.ui_version;
  if (text_input.unelision) {
    // Let the edit model unelide as well to match what we did on the
    // WebUI side.
    bool unelide_ok = controller()->edit_model()->Unelide();
    DCHECK(unelide_ok);
    // It should produce the same text (the 'formatted full URL').
    DCHECK_EQ(text_, text_input.text);

    // We want the WebUI-side selection, however, not Unelide()'s
    // SelectAll();
    selection_ = text_input.selection;
    TextChanged();
    RequestUpdateWebUI();
  } else {
    OnBeforePossibleChange();
    bool keep_additional_text =
        text_ + inline_autocompletion_ ==
        text_input.text + text_input.inline_autocompletion;
    SetTextAndSelectedRange(text_input.text, text_input.inline_autocompletion,
                            text_input.selection, keep_additional_text);
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
      if (omnibox::kShowRhsAimHint.Get()) {
#if BUILDFLAG(IS_MAC)
        const bool ai_mode_modifier = command;
#else
        const bool ai_mode_modifier = control;
#endif
        if (ai_mode_modifier && !shift) {
          controller()->edit_model()->OpenAiMode(
              OmniboxEditModel::AimActivation::kKeyboard);
          return base::ok(std::monostate());
        }
      }

      WindowOpenDisposition disposition =
          searchbox::ComputeOpenDispositionFromModifiersAndLogToUma(
              shift, control, alt, command);
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

    case ui::DomKey::DEL:
      DCHECK(shift);
      if (controller()->IsPopupOpen()) {
        controller()->edit_model()->TryDeletingPopupLine(
            controller()->edit_model()->GetPopupSelection().line);
      }
      break;

    case ui::DomKey::ARROW_UP:
      DCHECK(!shift);
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/false,
                                                    /*page=*/false);
      break;

    case ui::DomKey::ARROW_DOWN:
      DCHECK(!shift);
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/true,
                                                    /*page=*/false);
      break;

    case ui::DomKey::PAGE_UP:
      DCHECK(!control);
      DCHECK(!alt);
      DCHECK(!shift);
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/false,
                                                    /*page=*/true);
      break;

    case ui::DomKey::PAGE_DOWN:
      DCHECK(!control);
      DCHECK(!alt);
      DCHECK(!shift);
      controller()->edit_model()->OnUpOrDownPressed(/*down=*/true,
                                                    /*page=*/true);
      break;

    case ui::DomKey::FromCharacter(' '):
      if (aim_page_action_icon_has_fake_focus_) {
        if (base::FeatureList::IsEnabled(
                omnibox::kAiModeSpaceDoesNotActivate)) {
          ApplyFocusRingToAimButton(false);
          // The JS side will apply space.
        } else {
          controller()->edit_model()->OpenSelection(
              OmniboxPopupSelection(
                  OmniboxPopupSelection::kNoMatch,
                  OmniboxPopupSelection::LineState::FOCUSED_BUTTON_AIM),
              base::TimeTicks::Now(), WindowOpenDisposition::CURRENT_TAB,
              /*via_keyboard=*/true);
        }
        return base::ok(std::monostate());
      }

      if (controller()->IsPopupOpen() && !control && !alt && !shift) {
        // This is relying on search keyword activation incrementing browser
        // version to resolve the conflict with text input with ' ' appended
        // that's incoming --- the JS side doesn't know whether the space will
        // trigger the keyboard or not.
        if (controller()->edit_model()->OnSpacePressed()) {
          return base::ok(std::monostate());
        }
        OmniboxPopupSelection selection =
            controller()->edit_model()->GetPopupSelection();
        if (selection.IsButtonFocused()) {
          controller()->edit_model()->OpenSelection(
              selection, base::TimeTicks::Now(),
              WindowOpenDisposition::CURRENT_TAB, /*via_keyboard=*/true);
        }
      }
      break;

    case ui::DomKey::BACKSPACE:
      if (controller()->edit_model()->is_keyword_selected()) {
        controller()->edit_model()->ClearKeyword();
      }
      break;

    case ui::DomKey::TAB:
      if (controller()->IsPopupOpen()) {
        controller()->edit_model()->OnTabPressed(shift);
      }
      break;

    default:
      break;
  }
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnPointer(
    const toolbar_ui_api::mojom::OmniboxActionPointer& pointer) {
  // Either pointer up or pointer down permit launches.
  ExternalProtocolHandler::PermitLaunchUrl();
  bool is_down = pointer.is_pointer_down;
  bool start_zero_suggest = pointer.start_zero_suggest;

  if (is_down) {
    // Pointer down clears the pseudo-focus the popup has.
    if (controller()->IsPopupOpen()) {
      OmniboxPopupSelection selection =
          controller()->edit_model()->GetPopupSelection();
      if (selection.state != OmniboxPopupSelection::KEYWORD_MODE) {
        selection.state = OmniboxPopupSelection::NORMAL;
        controller()->edit_model()->SetPopupSelection(selection);
      }
    }
  } else {
    selection_ = pointer.selection;
    // Pointer up may start zero-suggest.
    if (start_zero_suggest) {
      controller()->edit_model()->StartZeroSuggestRequest();
    }
    update_propagator_->OpenOmniboxIfFullPopup(start_zero_suggest);
    if (base::FeatureList::IsEnabled(
            omnibox::kWebUIOmniboxFullPopupDoubleClick) &&
        location_bar_) {
      location_bar_->GetOmniboxPopupView()->SyncNativeStateToWebUI(
          start_zero_suggest);
    }
  }
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnDropText(
    const toolbar_ui_api::mojom::OmniboxActionDropText& drop_text) {
  std::u16string text = omnibox::StripJavascriptSchemas(
      base::CollapseWhitespace(drop_text.text, true));
  base::TrimWhitespace(text, base::TRIM_ALL, &text);

  SetUserText(text, /*update_popup=*/false);
  SelectAll(false);
  return base::ok(std::monostate());
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIReadOnlyOmnibox::OnDropFile(
    const toolbar_ui_api::mojom::OmniboxActionDropFile& drop_file) {
  if (std::optional<GURL> url =
          toolbar_delegate_->ConsumeDroppedUrl(drop_file.drop_position)) {
    std::u16string text = base::UTF8ToUTF16(url->spec());
    if (!text.empty()) {
      SetUserText(text, /*update_popup=*/false);
      SelectAll(false);
    }
  }
  return base::ok(std::monostate());
}

void WebUIReadOnlyOmnibox::OnContextMenuReady(
    views::Widget* widget,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  AddTextfieldItems(toolbar_delegate_->GetWebContents()->GetWeakPtr(),
                    menu_params_, menu_model_.get());
  AddOmniboxSpecificItems(menu_model_.get());

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(widget, /*button_controller=*/nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
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

void WebUIReadOnlyOmnibox::OnTemplateURLServiceChanged() {
  RequestUpdateWebUI();  // for placeholder text
}

void WebUIReadOnlyOmnibox::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Update the placeholder text after primary main frame navigation of the
  // currently displayed WebContents to ensure it reflects the current page,
  // including cases like back/forward navigation between the New Tab Page and
  // Contextual Tasks page.
  if (!location_bar_ || location_bar_->GetWebContents() != web_contents()) {
    return;
  }

  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    RequestUpdateWebUI();  // for placeholder text
  }
}

void WebUIReadOnlyOmnibox::TitleWasSet(content::NavigationEntry* entry) {
  // Update the placeholder text after title changes in the primary main frame
  // of the currently displayed WebContents.
  // For Contextual Tasks page, updates the placeholder text to the page title.
  if (!location_bar_ || location_bar_->GetWebContents() != web_contents()) {
    return;
  }

  if (entry &&
      entry == web_contents()->GetController().GetLastCommittedEntry()) {
    RequestUpdateWebUI();  // for placeholder text
  }
}
