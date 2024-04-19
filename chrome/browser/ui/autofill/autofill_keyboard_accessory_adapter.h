// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/common/aliases.h"

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace autofill {

class AutofillKeyboardAccessoryView;

// This adapter allows the AutofillSuggestionController to treat the keyboard
// accessory like any other implementation of AutofillPopupView.
// From the controller's perspective, this behaves like a real AutofillPopupView
// and for the view, it behaves like the real AutofillSuggestionController.
class AutofillKeyboardAccessoryAdapter
    : public AutofillPopupView,
      public AutofillKeyboardAccessoryController {
 public:
  explicit AutofillKeyboardAccessoryAdapter(
      base::WeakPtr<AutofillKeyboardAccessoryController> controller);
  AutofillKeyboardAccessoryAdapter(const AutofillKeyboardAccessoryAdapter&) =
      delete;
  AutofillKeyboardAccessoryAdapter& operator=(
      const AutofillKeyboardAccessoryAdapter&) = delete;

  ~AutofillKeyboardAccessoryAdapter() override;

  void SetAccessoryView(std::unique_ptr<AutofillKeyboardAccessoryView> view) {
    view_ = std::move(view);
  }

  base::WeakPtr<AutofillKeyboardAccessoryAdapter> GetWeakPtrToAdapter() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // AutofillPopupView:
  base::WeakPtr<AutofillPopupView> GetWeakPtr() override;

 private:
  // AutofillPopupView:
  bool Show(AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void Hide() override;
  bool OverlapsWithPictureInPictureWindow() const override;
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void OnSuggestionsChanged() override;
  void AxAnnounce(const std::u16string& text) override;
  std::optional<int32_t> GetAxUniqueId() override;
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillSuggestionController> controller) override;

  // AutofillSuggestionController:
  // Hidden: void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index) override;
  int GetLineCount() const override;
  const autofill::Suggestion& GetSuggestionAt(int row) const override;
  bool RemoveSuggestion(
      int index,
      AutofillMetrics::SingleEntryRemovalMethod removal_method) override;
  void SelectSuggestion(int index) override;
  void UnselectSuggestion() override;
  FillingProduct GetMainFillingProduct() const override;
  void Hide(PopupHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;
  std::vector<Suggestion> GetSuggestions() const override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  void Show(std::vector<Suggestion> suggestions,
            AutofillSuggestionTriggerSource trigger_source,
            AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void DisableThresholdForTesting(bool disable_threshold) override;
  void KeepPopupOpenForTesting() override;
  void SetViewForTesting(base::WeakPtr<AutofillPopupView> view) override;
  void UpdateDataListValues(base::span<const SelectOption> options) override;
  void PinView() override;

  // AutofillKeyboardAccessoryController:
  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override;
  bool GetRemovalConfirmationText(int index,
                                  std::u16string* title,
                                  std::u16string* body) override;
  base::WeakPtr<AutofillKeyboardAccessoryController> GetWeakPtrToController()
      override;

  void OnDeletionDialogClosed(int index, bool confirmed);

  base::WeakPtr<AutofillKeyboardAccessoryController> controller_;
  std::unique_ptr<AutofillKeyboardAccessoryView> view_;

  base::WeakPtrFactory<AutofillKeyboardAccessoryAdapter> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_
