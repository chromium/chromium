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
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/common/aliases.h"

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace autofill {

// This adapter allows the AutofillPopupController to treat the keyboard
// accessory like any other implementation of AutofillPopupView.
// From the controller's perspective, this behaves like a real AutofillPopupView
// and for the view, it behaves like the real AutofillPopupController.
class AutofillKeyboardAccessoryAdapter : public AutofillPopupView,
                                         public AutofillPopupController {
 public:
  AutofillKeyboardAccessoryAdapter(
      base::WeakPtr<AutofillPopupController> controller);

  AutofillKeyboardAccessoryAdapter(const AutofillKeyboardAccessoryAdapter&) =
      delete;
  AutofillKeyboardAccessoryAdapter& operator=(
      const AutofillKeyboardAccessoryAdapter&) = delete;

  ~AutofillKeyboardAccessoryAdapter() override;

  // Interface describing the minimal capabilities for the native view.
  class AccessoryView {
   public:
    virtual ~AccessoryView() = default;

    // Initializes the Java-side of this bridge. Returns true after a successful
    // creation and false otherwise.
    virtual bool Initialize() = 0;

    // Requests to dismiss this view.
    virtual void Hide() = 0;

    // Requests to show this view with the data provided by the controller.
    virtual void Show() = 0;

    // Makes announcement for acessibility.
    virtual void AxAnnounce(const std::u16string& text);

    // Ask to confirm a deletion. Triggers the callback upon confirmation.
    virtual void ConfirmDeletion(const std::u16string& confirmation_title,
                                 const std::u16string& confirmation_body,
                                 base::OnceClosure confirm_deletion) = 0;
  };

  void SetAccessoryView(std::unique_ptr<AccessoryView> view) {
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
  absl::optional<int32_t> GetAxUniqueId() override;
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> controller) override;

  // AutofillPopupController:
  // Hidden: void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index, base::TimeTicks event_time) override;
  void AcceptSuggestionWithoutThreshold(int index) override;
  int GetLineCount() const override;
  const autofill::Suggestion& GetSuggestionAt(int row) const override;
  std::u16string GetSuggestionMainTextAt(int row) const override;
  std::u16string GetSuggestionMinorTextAt(int row) const override;
  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override;
  bool GetRemovalConfirmationText(int index,
                                  std::u16string* title,
                                  std::u16string* body) override;
  bool RemoveSuggestion(int index) override;
  void SelectSuggestion(absl::optional<size_t> index) override;
  PopupType GetPopupType() const override;
  AutofillSuggestionTriggerSource GetAutofillSuggestionTriggerSource()
      const override;
  bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const override;
  base::WeakPtr<AutofillPopupController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions) override;
  void HideSubPopup() override;
  void Hide(PopupHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;
  std::vector<Suggestion> GetSuggestions() const override;

  void OnDeletionConfirmed(int index);

  // Indices might be offset because a special item is moved to the front. This
  // method returns the index used by the keyboard accessory (may be offset).
  // `element_index` is the position of an element as returned by `controller_`.
  int OffsetIndexFor(int element_index) const;

  base::WeakPtr<AutofillPopupController> controller_;
  std::unique_ptr<AutofillKeyboardAccessoryAdapter::AccessoryView> view_;

  // The labels to be used for the input chips.
  std::vector<std::u16string> labels_;

  // Position that the front element has in the suggestion list returned by
  // controller_. It is used to determine the offset suggestions.
  absl::optional<int> front_element_;

  base::WeakPtrFactory<AutofillKeyboardAccessoryAdapter> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_
