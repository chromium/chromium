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
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"

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

  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::SetCaretPos(size_t caret_pos) {
  selection_ = gfx::Range(caret_pos);

  RequestUpdateWebUI();
}

void WebUIReadOnlyOmnibox::SetAdditionalText(
    const std::u16string& additional_text) {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::EnterKeywordModeForDefaultSearchProvider() {
  NOTIMPLEMENTED();
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
}

void WebUIReadOnlyOmnibox::SetFocus(bool is_user_initiated) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& user_text,
    const std::u16string& inline_autocompletion) {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::OnInlineAutocompleteTextCleared() {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::OnRevertTemporaryText(
    const std::u16string& display_text,
    const AutocompleteMatch& match) {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::OnBeforePossibleChange() {
  NOTIMPLEMENTED();
}

bool WebUIReadOnlyOmnibox::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  NOTIMPLEMENTED();
  return false;
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
  if (selection_.IsValid()) {
    state->selection = selection_;
  }
  state->text_is_url = text_is_url_;

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
