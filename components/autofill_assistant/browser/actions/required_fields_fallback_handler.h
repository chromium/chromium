// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_

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
#include "components/autofill_assistant/browser/batch_element_checker.h"

namespace autofill_assistant {
class ClientStatus;

// A handler for required fields and fallback values, used for example in
// UseAddressAction.
class RequiredFieldsFallbackHandler {
 public:
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };
  struct RequiredField {
    Selector selector;
    bool simulate_key_presses = false;
    int delay_in_millisecond = 0;
    bool forced = false;
    FieldValueStatus status = UNKNOWN;

    int fallback_key;

    // Returns true if fallback is required for this field.
    bool ShouldFallback(bool has_fallback_data) const {
      return status == EMPTY || (forced && has_fallback_data);
    }
  };

  // Data necessary for filling in the fallback fields. This is kept in a
  // separate struct to make sure we don't keep it for longer than strictly
  // necessary.
  // TODO(marianfe): Refactor this to use a map instead.
  struct FallbackData {
    FallbackData();
    ~FallbackData();

    // the key of the map should be the same as the |fallback_key| of the
    // required field.
    std::map<int, std::string> field_values;

    base::Optional<std::string> GetValue(int key);

   private:
    DISALLOW_COPY_AND_ASSIGN(FallbackData);
  };

  explicit RequiredFieldsFallbackHandler(
      const std::vector<RequiredField>& required_fields,
      ActionDelegate* delegate);
  ~RequiredFieldsFallbackHandler();

  // Check if there are required fields. If so, verify them and fallback if
  // they are empty. If not, update the status to the result of the autofill
  // action.
  void CheckAndFallbackRequiredFields(
      const ClientStatus& initial_autofill_status,
      std::unique_ptr<FallbackData> fallback_data,
      base::OnceCallback<void(const ClientStatus&,
                              const base::Optional<ClientStatus>&)>
          status_update_callback);

 private:
  // Check whether all required fields have a non-empty value. If it is the
  // case, update the status to success. If it's not and |fallback_data|
  // is null, update the status to failure. If |fallback_data| is non-null, use
  // it to attempt to fill the failed fields without Autofill.
  void CheckAllRequiredFields(std::unique_ptr<FallbackData> fallback_data);

  // Triggers the check for a specific field.
  void CheckRequiredFieldsSequentially(
      bool allow_fallback,
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Updates the status of the required field.
  void OnGetRequiredFieldValue(size_t required_fields_index,
                               const ClientStatus& element_status,
                               const std::string& value);

  // Called when all required fields have been checked.
  void OnCheckRequiredFieldsDone(std::unique_ptr<FallbackData> fallback_data);

  // Sets fallback field values for empty fields.
  void SetFallbackFieldValuesSequentially(
      size_t required_fields_index,
      std::unique_ptr<FallbackData> fallback_data);

  // Called after trying to set form values without Autofill in case of fallback
  // after failed validation.
  void OnSetFallbackFieldValue(size_t required_fields_index,
                               std::unique_ptr<FallbackData> fallback_data,
                               const ClientStatus& status);

  ClientStatus initial_autofill_status_;

  std::vector<RequiredField> required_fields_;
  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      status_update_callback_;
  ActionDelegate* action_delegate_;
  std::unique_ptr<BatchElementChecker> batch_element_checker_;
  base::WeakPtrFactory<RequiredFieldsFallbackHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RequiredFieldsFallbackHandler);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REQUIRED_FIELDS_FALLBACK_HANDLER_H_
