// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_READONLY_OMNIBOX_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_READONLY_OMNIBOX_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}  // namespace content

// WebUI-implementation of OmniboxView, which happens to be readonly,
// as it counts on the popup to handle the editing.
// TODO(crbug.com/500653057): Rename it in a manner more consistent with other
// classes here. It's also no longer read-only!
class WebUIReadOnlyOmnibox : public OmniboxView {
 public:
  class UpdatePropagator {
   public:
    virtual ~UpdatePropagator();
    virtual void PropagateOmniboxUpdate(
        toolbar_ui_api::mojom::OmniboxViewStatePtr update) = 0;
  };

  // Both parameters must outlive `this`.
  WebUIReadOnlyOmnibox(OmniboxController* controller,
                       UpdatePropagator& update_propagator);
  WebUIReadOnlyOmnibox(const WebUIReadOnlyOmnibox&) = delete;
  WebUIReadOnlyOmnibox& operator=(const WebUIReadOnlyOmnibox&) = delete;
  ~WebUIReadOnlyOmnibox() override;

  // Called from the location bar.
  void SaveStateToTab(content::WebContents* tab);
  void OnTabChanged(const content::WebContents* web_contents);
  void ResetTabState(content::WebContents* web_contents);

  // OmniboxView:
  void Update() override;
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void SetAdditionalText(const std::u16string& text) override;
  void EnterKeywordModeForDefaultSearchProvider() override;
  bool IsSelectAll() const override;
  gfx::Range GetSelectionBounds() const override;
  void SelectAll(bool reversed) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void SetFocus(bool is_user_initiated) override;
  bool AimButtonVisible() const override;
  void ApplyCaretVisibility() override;
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override;
  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& range) override;

  toolbar_ui_api::mojom::OmniboxViewStatePtr ComputeMojoState() const;

 private:
  void RequestUpdateWebUI();
  void ResetFormatting();

  raw_ref<UpdatePropagator> update_propagator_;

  // Text and selection (or caret) we were asked to display (e.g. via
  // SetWindowTextAndCaretPos()) by either the base class or OmniboxEditModel.
  std::u16string text_;

  // Rich text formatting for `text`.
  gfx::BreakList<bool> text_strike_through_;
  gfx::BreakList<toolbar_ui_api::mojom::OmniboxTextColor> text_colors_;
  bool text_is_url_ = false;

  // When start and end positions match, this represents a caret position;
  // if they don't, it's a selection.
  gfx::Range selection_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_READONLY_OMNIBOX_H_
