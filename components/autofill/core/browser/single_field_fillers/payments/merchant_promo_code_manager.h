// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;

// Per-tab Merchant Promo Code Manager. This class handles promo code
// related functionality such as retrieving promo code offer data, managing
// promo code suggestions, filling promo code fields, and handling form
// submission metrics.
class MerchantPromoCodeManager : public AutofillManager::Observer {
 public:
  explicit MerchantPromoCodeManager(AutofillClient* autofill_client);

  MerchantPromoCodeManager(const MerchantPromoCodeManager&) = delete;
  MerchantPromoCodeManager& operator=(const MerchantPromoCodeManager&) = delete;

  ~MerchantPromoCodeManager() override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;
  void OnBeforeFormSubmitted(AutofillManager& manager,
                             const FormData& form) override;

  // May generate promo code suggestions for the given `autofill_field` which
  // belongs to the `form_structure`.
  // If `OnGetSingleFieldSuggestions` decides to claim the opportunity to fill
  // `field`, it returns true and calls `on_suggestions_returned`. Claiming the
  // opportunity is not a promise that suggestions will be available. The
  // callback may be called with no suggestions.
  [[nodiscard]] virtual bool OnGetSingleFieldSuggestions(
      const FormStructure& form_structure,
      const FormFieldData& field,
      const AutofillField& autofill_field,
      AutofillClient& client,
      SingleFieldFillRouter::OnSuggestionsReturnedCallback&
          on_suggestions_returned);

  // Logs promo code suggestion filled funnel event (at most once per page
  // load). `suggestion` must be a merchant promo code entry.
  virtual void OnSingleFieldSuggestionSelected(const Suggestion& suggestion);

  // Logs promo code suggestions shown funnel event (at most once per page
  // load).
  virtual void DidShowSuggestions();

  // Resets page-scoped metrics state. Called on navigation since
  // `MerchantPromoCodeManager` is per-tab.
  virtual void Reset();

 private:
  struct PageMetrics {
    // Indicates whether the page load metric has already been logged for the
    // current page load. This metric is only logged on pages with a promo code
    // field, and records whether the user has a promo code saved for the page.
    bool has_logged_if_promo_code_exists_for_page = false;

    // Indicates whether promo code suggestions shown metric has already been
    // logged for the current page load.
    bool has_logged_suggestions_shown = false;

    // Indicates whether promo code suggestion filled metric has already been
    // logged for the current page load.
    bool has_logged_suggestion_filled = false;

    // Indicates whether form submitted metric has already been logged for the
    // current page load.
    bool has_logged_form_submitted = false;
  };

  // Returns whether any promo code field in the forms tracked by
  // `promo_code_form_global_ids_` contains a non-whitespace value. Values of
  // fields in `submitted_form` are read from it, since the cached form is not
  // updated with the submitted values yet when the form is submitted.
  bool HasPromoCodeFieldWithValue(AutofillManager& manager,
                                  const FormData& submitted_form) const;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  // Tracks the FormGlobalIds of all forms containing a visible promo code field
  // on the current page. Empty if no promo code field has been detected.
  base::flat_set<FormGlobalId> promo_code_form_global_ids_;

  PageMetrics page_metrics_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_PAYMENTS_MERCHANT_PROMO_CODE_MANAGER_H_
