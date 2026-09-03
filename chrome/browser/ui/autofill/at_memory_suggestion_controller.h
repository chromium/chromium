// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AT_MEMORY_SUGGESTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AT_MEMORY_SUGGESTION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AtMemoryBottomSheetBridge;
class AutofillSuggestionDelegate;

// Controller for the AtMemory suggestion flow on Android.
class AtMemorySuggestionController : public AutofillSuggestionController {
 public:
  AtMemorySuggestionController(const AtMemorySuggestionController&) = delete;
  AtMemorySuggestionController& operator=(const AtMemorySuggestionController&) =
      delete;

  AtMemorySuggestionController(
      base::WeakPtr<AutofillSuggestionDelegate> delegate,
      content::WebContents* web_contents,
      PopupControllerCommon controller_common);

  base::WeakPtr<AtMemorySuggestionController> GetWeakPtr();

  // AutofillPopupViewDelegate:
  void Hide(SuggestionHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  PopupAnchorType anchor_type() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // AutofillSuggestionController:
  void OnSuggestionsChanged() override;
  void AcceptSuggestion(
      int index,
      AutofillMetrics::SuggestionAcceptedMethod accept_method) override;
  void SelectSuggestion(int index) override;
  void UnselectSuggestion() override;
  bool RemoveSuggestion(int index) override;
  int GetLineCount() const override;
  const std::vector<Suggestion>& GetSuggestions() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  FillingProduct GetMainFillingProduct() const override;
  void Show(UiSessionId session_id,
            std::vector<Suggestion> suggestions,
            AutofillSuggestionTriggerSource trigger_source,
            AutoselectFirstSuggestion autoselect_first_suggestion,
            AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss,
            std::u16string search_bar_initial_value) override;
  std::optional<UiSessionId> GetUiSessionId() const override;
  void SetKeepPopupOpenForTesting(bool keep_popup_open_for_testing) override;
  void UpdateDataListValues(base::span<const SelectOption> options) override;
  bool MayRecycle(
      base::WeakPtr<AutofillSuggestionDelegate> delegate,
      content::WebContents* web_contents,
      AutofillSuggestionTriggerSource trigger_source) const override;
  void Recycle(PopupControllerCommon controller_common,
               int32_t form_control_ax_id) override;

  virtual void OnDismissed();
  void OnQuerySubmitted(const std::u16string& query);
  void OnQueryTextChanged(const std::u16string& query);
  void OnSuggestionSelected(int position);
  void OnSuggestionDismissed(int position);
  void OnChildSuggestionsShown(int parent_position);
  void OnChildSuggestionSelected(int parent_position, int child_position);
  bool IsSearching() const;

  void SetBridgeForTesting(std::unique_ptr<AtMemoryBottomSheetBridge> bridge);
  AtMemoryBottomSheetBridge* bridge_for_testing() const;

 protected:
  ~AtMemorySuggestionController() override;

 private:
  void HideViewAndDie();

  base::WeakPtr<AutofillSuggestionDelegate> delegate_;
  base::WeakPtr<content::WebContents> web_contents_;
  PopupControllerCommon controller_common_;
  std::optional<UiSessionId> ui_session_id_;
  std::vector<Suggestion> suggestions_;
  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;
  std::unique_ptr<AtMemoryBottomSheetBridge> bridge_;

  base::WeakPtrFactory<AtMemorySuggestionController>
      self_deletion_weak_ptr_factory_{this};
  base::WeakPtrFactory<AtMemorySuggestionController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AT_MEMORY_SUGGESTION_CONTROLLER_H_
