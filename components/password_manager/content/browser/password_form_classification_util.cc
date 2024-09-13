// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_form_classification_util.h"

#include "base/ranges/ranges.h"
#include "components/autofill/content/browser/renderer_forms_with_server_predictions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/global_routing_id.h"

namespace password_manager {

autofill::PasswordFormClassification ClassifyAsPasswordForm(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id) {
  // Find the form with `form_id` and decompose into renderer forms.
  std::optional<autofill::RendererFormsWithServerPredictions>
      forms_and_predictions =
          autofill::RendererFormsWithServerPredictions::FromBrowserForm(
              manager, form_id);
  if (!forms_and_predictions) {
    return {};
  }

  // Find the form to which `field_id` belongs.
  auto it = base::ranges::find_if(
      forms_and_predictions->renderer_forms,
      [field_id](
          const std::pair<autofill::FormData, content::GlobalRenderFrameHostId>&
              form_rfh_pair) {
        const autofill::FormData& form = form_rfh_pair.first;
        return base::ranges::find(form.fields(), field_id,
                                  &autofill::FormFieldData::global_id) !=
               form.fields().end();
      });
  if (it == forms_and_predictions->renderer_forms.end()) {
    return {};
  }

  FormDataParser parser;
  // The driver id is irrelevant here because it would only be used by password
  // manager logic that handles the `PasswordForm` returned by the parser.
  parser.set_predictions(ConvertToFormPredictions(
      /*driver_id=*/0, it->first, forms_and_predictions->predictions));
  // The parser can use stored usernames to identify a filled username field by
  // the value it contains. Here it remains empty.
  std::unique_ptr<PasswordForm> pw_form =
      parser.Parse(it->first, FormDataParser::Mode::kFilling,
                   /*stored_usernames=*/{});
  if (!pw_form) {
    return {};
  }
  autofill::PasswordFormClassification result{
      .type = pw_form->GetPasswordFormType()};
  if (!pw_form->username_element_renderer_id.is_null()) {
    result.username_field = autofill::FieldGlobalId(
        field_id.frame_token, pw_form->username_element_renderer_id);
  }
  return result;
}

}  // namespace password_manager
