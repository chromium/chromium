// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_

#include <string>
#include <vector>

#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakeMojoPasswordManagerDriver
    : public autofill::mojom::PasswordManagerDriver {
 public:
  FakeMojoPasswordManagerDriver();

  ~FakeMojoPasswordManagerDriver() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          receiver);

  // Flushes all pending messages from the associated binding.
  void Flush();

  // mojom::PasswordManagerDriver:
  MOCK_METHOD(void,
              PasswordFormCleared,
              (const autofill::FormData&),
              (override));
  MOCK_METHOD(void, UserModifiedPasswordField, (), (override));
  MOCK_METHOD(void,
              UserModifiedNonPasswordField,
              (autofill::FieldRendererId renderer_id,
               const std::u16string& value,
               bool autocomplete_attribute_has_username,
               bool is_likely_otp),
              (override));
  MOCK_METHOD(void,
              ShowPasswordSuggestions,
              (const autofill::PasswordSuggestionRequest& request),
              (override));
  MOCK_METHOD(void,
              PasswordFormSubmitted,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(
      void,
      DynamicFormSubmission,
      (autofill::mojom::SubmissionIndicatorEvent submission_indication_event),
      (override));
  MOCK_METHOD(void,
              PasswordFormsParsed,
              (const std::vector<autofill::FormData>& forms_data),
              (override));
  MOCK_METHOD(void,
              PasswordFormsRendered,
              (const std::vector<autofill::FormData>& visible_forms_data),
              (override));
  MOCK_METHOD(void,
              RecordSavePasswordProgress,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL& form_action, const GURL& frame_url),
              (override));
  MOCK_METHOD(void,
              InformAboutUserInput,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(void,
              FocusedInputChanged,
              (autofill::FieldRendererId focused_field_id,
               autofill::mojom::FocusedFieldType focused_field_type),
              (override));
  MOCK_METHOD(void,
              LogFirstFillingResult,
              (autofill::FormRendererId form_renderer_id, int32_t result),
              (override));

 private:
  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver> receiver_{
      this};
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
