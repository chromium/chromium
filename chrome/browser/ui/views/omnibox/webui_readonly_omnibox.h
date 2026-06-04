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
#include <variant>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/events/keycodes/dom/dom_key.h"
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
    virtual void PropagateFocusRequest(
        toolbar_ui_api::mojom::FocusRequestTarget target) = 0;
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
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnOmniboxAction(
      toolbar_ui_api::mojom::OmniboxActionPtr action);

  // Updates the state of the display stored in `this` OmniboxView. Doesn't
  // notify the OmniboxEditModel or the WebUI end.
  void SetTextAndSelectedRange(const std::u16string& text,
                               const std::u16string& inline_autocompletion,
                               const gfx::Range& selection);

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

  // Requests focus with particular omnibox-related target
  void SetFocusWithTarget(toolbar_ui_api::mojom::FocusRequestTarget target);

 private:
  void RequestUpdateWebUI();
  void ResetFormatting();
  void ResetBrowserVersion();

  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnFocusChange(
      const toolbar_ui_api::mojom::OmniboxActionFocusChange& focus_change);
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnTextInput(
      const toolbar_ui_api::mojom::OmniboxActionTextInput& text_input);
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnKey(
      const toolbar_ui_api::mojom::OmniboxActionKey& key);

  ui::DomKey LookupAndCacheDomKey(std::string_view key_str);

  raw_ref<UpdatePropagator> update_propagator_;

  absl::flat_hash_map<std::string, ui::DomKey> key_code_cache_;

  // Versions of the text on both ends. The browser end increments its version
  // number when the entire URL is reset, due to navigation or interaction
  // with popup; the UI end increments it on user user input.
  uint32_t ui_version_ = 0;
  uint32_t browser_version_ = 0;

  // Text and selection (or caret) we were asked to display (e.g. via
  // SetWindowTextAndCaretPos()) by either the base class or OmniboxEditModel.
  std::u16string text_;

  // Inline completion suggested by auto-complete.
  std::u16string inline_autocompletion_;

  // An additional description for what's being displayed.
  std::u16string additional_text_;

  // Rich text formatting for `text`.
  gfx::BreakList<bool> text_strike_through_;
  gfx::BreakList<toolbar_ui_api::mojom::OmniboxTextColor> text_colors_;
  bool text_is_url_ = false;

  // When start and end positions match, this represents a caret position;
  // if they don't, it's a selection.
  gfx::Range selection_;

  // Selection saved when temporary text is being displayed, so it can be
  // restored if the user presses `Esc` to cancel it.
  gfx::Range saved_selection_for_temporary_text_;

  // State of the world at the time of last call to `OnBeforePossibleChange()`;
  // used in `OnAfterPossibleChange()` to figure out what changed.
  State state_before_change_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_READONLY_OMNIBOX_H_
