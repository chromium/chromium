// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace autofill {

class AutofillBubbleBase;
class PaymentsDataManager;
enum class PaymentsUiClosedReason;
enum class SuggestionHidingReason;

// Controller class that exposes functionality to omnibox autofill bubbles.
// Owned by TabFeatures.
class OmniboxAutofillBubbleController : public AutofillBubbleControllerBase {
 public:
  DECLARE_USER_DATA(OmniboxAutofillBubbleController);

  explicit OmniboxAutofillBubbleController(tabs::TabInterface& tab_interface,
                                           content::WebContents* web_contents);
  OmniboxAutofillBubbleController(const OmniboxAutofillBubbleController&) =
      delete;
  OmniboxAutofillBubbleController& operator=(
      const OmniboxAutofillBubbleController&) = delete;
  ~OmniboxAutofillBubbleController() override;

  static OmniboxAutofillBubbleController* From(
      tabs::TabInterface& tab_interface);

  // BubbleControllerBase:
  void OnBubbleDiscarded() override {}
  BubbleType GetBubbleType() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

  // Initializes the controller with suggestions and callbacks.
  void Initialize(
      std::vector<Suggestion> suggestions,
      base::RepeatingCallback<void(base::span<const Suggestion>)>
          on_suggestions_shown,
      base::RepeatingCallback<void(SuggestionHidingReason)>
          on_suggestions_hidden,
      base::RepeatingCallback<void(const Suggestion&)> did_select_suggestion,
      base::RepeatingClosure did_deselect_suggestion,
      base::RepeatingCallback<
          void(const Suggestion&,
               const AutofillSuggestionDelegate::SuggestionMetadata&)>
          did_accept_suggestion);

  AutofillBubbleBase* GetBubbleView() const;
  std::u16string GetWindowTitle() const;
  const std::vector<Suggestion>& GetSuggestions() const;
  base::WeakPtr<OmniboxAutofillBubbleController> GetWeakPtr();

  void OnSuggestionsShown();
  void OnSuggestionDeselected();
  void OnBubbleClosed(PaymentsUiClosedReason reason);
  void OnSuggestionSelected(const Suggestion& suggestion);
  void OnSuggestionAccepted(const Suggestion& suggestion, size_t row);

  bool ShouldShowGooglePayLogo() const;

 protected:
  void DoShowBubble() override;

 private:
  ui::ScopedUnownedUserData<OmniboxAutofillBubbleController>
      scoped_unowned_user_data_;

  const raw_ref<PaymentsDataManager> payments_data_manager_;

  std::vector<Suggestion> suggestions_;
  base::RepeatingCallback<void(base::span<const Suggestion>)>
      on_suggestions_shown_callback_;
  base::RepeatingCallback<void(SuggestionHidingReason)>
      on_suggestions_hidden_callback_;
  base::RepeatingCallback<void(const Suggestion&)>
      did_select_suggestion_callback_;
  base::RepeatingClosure did_deselect_suggestion_callback_;
  base::RepeatingCallback<void(
      const Suggestion&,
      const AutofillSuggestionDelegate::SuggestionMetadata&)>
      did_accept_suggestion_callback_;

  base::WeakPtrFactory<OmniboxAutofillBubbleController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_
