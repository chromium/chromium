// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELDS_FALLBACK_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
class ClientStatus;

// A handler for required fields and fallback values, used by UseAddressAction
// and UseCreditCardAction.
class RequiredFieldsFallbackHandler {
 public:
  explicit RequiredFieldsFallbackHandler(
      const std::vector<RequiredField>& required_fields,
      const std::map<std::string, std::string>& fallback_values,
      ActionDelegate* delegate);

  ~RequiredFieldsFallbackHandler();

  // Check if there are required fields. If so, verify them and fallback if
  // they are empty. If not, update the status to the result of the autofill
  // action.
  void CheckAndFallbackRequiredFields(
      const ClientStatus& initial_autofill_status,
      base::OnceCallback<void(const ClientStatus&,
                              const base::Optional<ClientStatus>&)>
          status_update_callback);

 private:
  // Check whether all required fields have a non-empty value. If it is the
  // case, update the status to success. If it's not and |apply_fallback|
  // is false, update the status to failure. If |apply_fallback| is true,
  // attempt to fill the failed fields without Autofill using fallback values.
  void CheckAllRequiredFields(bool apply_fallback);

  // Triggers the check for a specific field.
  void CheckRequiredFieldsSequentially(bool allow_fallback,
                                       size_t required_fields_index);

  // Updates the status of the required field.
  void OnGetRequiredFieldValue(size_t required_fields_index,
                               const ClientStatus& element_status,
                               const std::string& value);

  // Called when all required fields have been checked.
  void OnCheckRequiredFieldsDone(bool apply_fallback);

  // Sets fallback field values for empty fields.
  void SetFallbackFieldValuesSequentially(size_t required_fields_index);

  // Called after attempting to find one of the elements to execute a fallback
  // action on.
  void OnFindElement(const std::string& value,
                     size_t required_fields_index,
                     const ClientStatus& element_status,
                     std::unique_ptr<ElementFinder::Result> element_result);

  // Called after retrieving tag name from a field.
  void OnGetFallbackFieldElementTag(
      const std::string& value,
      size_t required_fields_index,
      std::unique_ptr<ElementFinder::Result> element,
      const ClientStatus& element_tag_status,
      const std::string& element_tag);

  // Called after clicking a fallback element.
  void OnClickOrTapFallbackElement(const std::string& value,
                                   size_t required_fields_index,
                                   const ClientStatus& element_click_status);
  // Called after waiting for option element to appear before clicking it.
  void OnShortWaitForElement(const Selector& selector_to_click,
                             size_t required_fields_index,
                             const ClientStatus& find_element_status);

  // Called after trying to set form values without Autofill in case of
  // fallback after failed validation.
  void OnSetFallbackFieldValue(size_t required_fields_index,
                               std::unique_ptr<ElementFinder::Result> element,
                               const ClientStatus& status);

  ClientStatus client_status_;

  std::vector<RequiredField> required_fields_;
  std::map<std::string, std::string> fallback_values_;
  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      status_update_callback_;
  ActionDelegate* action_delegate_;
  std::unique_ptr<BatchElementChecker> batch_element_checker_;
  base::WeakPtrFactory<RequiredFieldsFallbackHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RequiredFieldsFallbackHandler);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
