// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_

#include <stddef.h>

#include <string>

#include "components/omnibox/browser/omnibox_view.h"
#include "ui/gfx/range/range.h"

struct AutocompleteMatch;

// Fake implementation of OmniboxView for use in tests.
class TestOmniboxView : public OmniboxView {
 public:
  explicit TestOmniboxView(std::unique_ptr<OmniboxClient> client)
      : OmniboxView(std::move(client)) {}

  TestOmniboxView(const TestOmniboxView&) = delete;
  TestOmniboxView& operator=(const TestOmniboxView&) = delete;

  const std::u16string& inline_autocompletion() const {
    return inline_autocompletion_;
  }

  static State CreateState(std::string text,
                           size_t sel_start,
                           size_t sel_end,
                           size_t all_sel_length);

  // OmniboxView:
  void Update() override {}
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override {}
  void SetAdditionalText(const std::u16string& text) override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(size_t* start, size_t* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override;
  void UpdatePopup() override {}
  void SetFocus(bool is_user_initiated) override {}
  void ApplyCaretVisibility() override {}
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& display_text,
      std::vector<gfx::Range> selections,
      const std::u16string& prefix_autocompletion,
      const std::u16string& inline_autocompletion) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override {}
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  bool IsImeComposing() const override;
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& range) override {}
  using OmniboxView::GetStateChanges;

 private:
  std::u16string text_;
  std::u16string inline_autocompletion_;
  gfx::Range selection_;
  gfx::Range saved_temporary_selection_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_VIEW_H_
