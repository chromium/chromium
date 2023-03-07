// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Enum that describes different outcomes to an attempt of triggering the
// Touch To Fill bottom sheet for credit cards.
// The enum values are not exhaustive to avoid excessive metric collection.
// The cases where TTF is not shown because of other form type (not credit card)
// or TTF being not supported are skipped.
// Do not remove or renumber entries in this enum. It needs to be kept in
// sync with the enum of the same name in `enums.xml`.
enum class TouchToFillCreditCardTriggerOutcome {
  // The sheet was shown.
  kShown = 0,
  // The sheet was not shown because the clicked field was not focusable or
  // already had a value.
  kFieldNotEmptyOrNotFocusable = 1,
  // The sheet was not shown because there were no valid credit cards to
  // suggest.
  kNoValidCards = 2,
  // The sheet was not shown because either the client or the form was not
  // secure.
  kFormOrClientNotSecure = 3,
  // The sheet was not shown because it has already been shown before.
  kShownBefore = 4,
  // The sheet was not shown because Autofill UI cannot be shown.
  kCannotShowAutofillUi = 5,
  // There was a try to display the bottom sheet, but it failed due to unknown
  // reason.
  kFailedToDisplayBottomSheet = 6,
  // The sheet was not shown because the payment form was incomplete.
  kIncompleteForm = 7,
  // The form or field is not known to the form cache.
  kUnknownForm = 8,
  // The form is known to the form cache, but it doesn't contain the field.
  kUnknownField = 9,
  // TouchToFill is not supported for this field type. This value is not logged
  // to UMA.
  kUnsupportedFieldType = 9,
  kMaxValue = kUnsupportedFieldType
};

constexpr const char kUmaTouchToFillCreditCardTriggerOutcome[] =
    "Autofill.TouchToFill.CreditCard.TriggerOutcome";

class AutofillManager;
class BrowserAutofillManager;

// Delegate for in-browser Touch To Fill (TTF) surface display and selection.
// Currently TTF surface is eligible only for credit card forms on click on
// an empty focusable field.
//
// If the surface was shown once, it won't be triggered again on the same page.
// But calling |Reset()| on navigation restores such showing eligibility.
//
// It is supposed to be owned by the given |BrowserAutofillManager|, and
// interact with it and its |AutofillClient| and |AutofillDriver|.
//
// Public methods are marked virtual for testing.
// TODO(crbug.com/1324900): Consider using more descriptive name.
class TouchToFillDelegateImpl : public TouchToFillDelegate {
 public:
  explicit TouchToFillDelegateImpl(BrowserAutofillManager* manager);
  TouchToFillDelegateImpl(const TouchToFillDelegateImpl&) = delete;
  TouchToFillDelegateImpl& operator=(const TouchToFillDelegateImpl&) = delete;
  ~TouchToFillDelegateImpl() override;

  // Checks whether TTF is eligible for the given web form data. On success
  // triggers the corresponding surface and returns |true|.
  virtual bool TryToShowTouchToFill(const FormData& form,
                                    const FormFieldData& field);

  // Returns whether the TTF surface is currently being shown.
  virtual bool IsShowingTouchToFill();

  // Hides the TTF surface if one is shown.
  virtual void HideTouchToFill();

  // Resets the delegate to its starting state (e.g. on navigation).
  virtual void Reset();

  // TouchToFillDelegate:
  AutofillManager* GetManager() override;
  bool ShouldShowScanCreditCard() override;
  void ScanCreditCard() override;
  void OnCreditCardScanned(const CreditCard& card) override;
  void ShowCreditCardSettings() override;
  void SuggestionSelected(std::string unique_id, bool is_virtual) override;
  void OnDismissed(bool dismissed_by_user) override;

  void LogMetricsAfterSubmission(const FormStructure& submitted_form) const;

  base::WeakPtr<TouchToFillDelegateImpl> GetWeakPtr();

 private:
  enum class TouchToFillState {
    kShouldShow,
    kIsShowing,
    kWasShown,
  };

  using TriggerOutcome = TouchToFillCreditCardTriggerOutcome;

  struct DryRunResult {
    DryRunResult(TriggerOutcome outcome,
                 std::vector<CreditCard> cards_to_suggest);
    DryRunResult(DryRunResult&&);
    DryRunResult& operator=(DryRunResult&&);
    ~DryRunResult();

    TriggerOutcome outcome;
    std::vector<CreditCard> cards_to_suggest;
  };

  // Checks all preconditions for showing the TTF, that is, for calling
  // AutofillClient::ShowTouchToFillCreditCard().
  //
  // If the DryRunResult::outcome is TriggerOutcome::kShow, the
  // DryRun::cards_to_suggest contains the cards; otherwise it is empty.
  DryRunResult DryRun(FormGlobalId form_id, FieldGlobalId field_id);

  // Sets whether or not to suppress the on-screen keyboard in following
  // requests that would usually display the keyboard.
  //
  // No-op if `suppress` if the previous call had the same value as `suppress`.
  void SetShouldSuppressKeyboard(bool suppress);

  bool HasAnyAutofilledFields(const FormStructure& submitted_form) const;

  // The form is considered perfectly filled if all non-empty fields are
  // autofilled without further edits.
  bool IsFillingPerfect(const FormStructure& submitted_form) const;

  // The form is considered correctly filled if all autofilled fields were not
  // edited by user afterwards.
  bool IsFillingCorrect(const FormStructure& submitted_form) const;

  TouchToFillState ttf_credit_card_state_ = TouchToFillState::kShouldShow;

  const raw_ptr<BrowserAutofillManager> manager_;
  bool keyboard_is_suppressed_ = false;
  FormData query_form_;
  FormFieldData query_field_;
  bool dismissed_by_user_;

  base::WeakPtrFactory<TouchToFillDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_
