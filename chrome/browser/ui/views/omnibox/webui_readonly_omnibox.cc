// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <memory>

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "content/public/browser/web_contents.h"

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

WebUIReadOnlyOmnibox::WebUIReadOnlyOmnibox(OmniboxController* controller,
                                           WebUILocationBar* location_bar)
    : OmniboxView(controller), selection_(gfx::Range::InvalidRange()) {}

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

  // TODO: Update WebUI.
}

void WebUIReadOnlyOmnibox::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxTabHelper::kOmniboxStateKey, nullptr);
}

void WebUIReadOnlyOmnibox::Update() {
  // TODO: Identical to OmniboxViewViews; need a sharing strategy.
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

  // TODO: update_popup?

  if (notify_text_changed) {
    TextChanged();
  }

  // TODO: Update WebUI.
}

void WebUIReadOnlyOmnibox::SetCaretPos(size_t caret_pos) {
  selection_ = gfx::Range(caret_pos);

  // TODO: Update WebUI.
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
  // TODO: Update WebUI.
}

void WebUIReadOnlyOmnibox::UpdatePopup() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::SetEmphasis(bool emphasize,
                                       const gfx::Range& range) {
  NOTIMPLEMENTED();
}

void WebUIReadOnlyOmnibox::UpdateSchemeStyle(const gfx::Range& range) {
  NOTIMPLEMENTED();
}
