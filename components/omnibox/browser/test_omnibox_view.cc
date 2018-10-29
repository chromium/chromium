// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_view.h"

#include <algorithm>

#include "ui/gfx/native_widget_types.h"

void TestOmniboxView::SetModel(std::unique_ptr<OmniboxEditModel> model) {
  model_ = std::move(model);
}

base::string16 TestOmniboxView::GetText() const {
  return text_;
}

void TestOmniboxView::SetWindowTextAndCaretPos(const base::string16& text,
                                               size_t caret_pos,
                                               bool update_popup,
                                               bool notify_text_changed) {
  text_ = text;
  selection_ = gfx::Range(caret_pos);
}

bool TestOmniboxView::IsSelectAll() const {
  return selection_.EqualsIgnoringDirection(gfx::Range(0, text_.size()));
}

void TestOmniboxView::GetSelectionBounds(size_t* start, size_t* end) const {
  *start = selection_.start();
  *end = selection_.end();
}

void TestOmniboxView::SelectAll(bool reversed) {
  if (reversed)
    selection_ = gfx::Range(text_.size(), 0);
  else
    selection_ = gfx::Range(0, text_.size());
}

void TestOmniboxView::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  text_ = display_text;
}

bool TestOmniboxView::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  const bool text_changed = text_ != display_text;
  text_ = display_text;
  inline_autocomplete_text_ = display_text.substr(user_text_length);
  selection_ = gfx::Range(text_.size(), user_text_length);
  return text_changed;
}

void TestOmniboxView::OnInlineAutocompleteTextCleared() {
  inline_autocomplete_text_.clear();
}

bool TestOmniboxView::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  return false;
}

gfx::NativeView TestOmniboxView::GetNativeView() const {
  return nullptr;
}

gfx::NativeView TestOmniboxView::GetRelativeWindowForPopup() const {
  return nullptr;
}

int TestOmniboxView::GetTextWidth() const {
  return 0;
}

int TestOmniboxView::GetWidth() const {
  return 0;
}

bool TestOmniboxView::IsImeComposing() const {
  return false;
}

int TestOmniboxView::GetOmniboxTextLength() const {
  return 0;
}
