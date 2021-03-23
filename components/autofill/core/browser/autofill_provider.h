// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_

#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"

namespace gfx {
class RectF;
}

namespace autofill {

class AutofillHandlerProxy;

// This class defines the interface for the autofill implementation other than
// default AutofillManager.
class AutofillProvider {
 public:
  AutofillProvider();
  virtual ~AutofillProvider();

  static bool is_download_manager_disabled_for_testing();
  static void set_is_download_manager_disabled_for_testing();

  virtual void OnQueryFormFieldAutofill(AutofillHandlerProxy* handler,
                                        int32_t id,
                                        const FormData& form,
                                        const FormFieldData& field,
                                        const gfx::RectF& bounding_box,
                                        bool autoselect_first_suggestion) = 0;

  virtual void OnTextFieldDidChange(AutofillHandlerProxy* handler,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box,
                                    const base::TimeTicks timestamp) = 0;

  virtual void OnTextFieldDidScroll(AutofillHandlerProxy* handler,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) = 0;

  virtual void OnSelectControlDidChange(AutofillHandlerProxy* handler,
                                        const FormData& form,
                                        const FormFieldData& field,
                                        const gfx::RectF& bounding_box) = 0;

  virtual void OnFormSubmitted(AutofillHandlerProxy* handler,
                               const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source) = 0;

  virtual void OnFocusNoLongerOnForm(AutofillHandlerProxy* handler,
                                     bool had_interacted_form) = 0;

  virtual void OnFocusOnFormField(AutofillHandlerProxy* handler,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const gfx::RectF& bounding_box) = 0;

  virtual void OnDidFillAutofillFormData(AutofillHandlerProxy* handler,
                                         const FormData& form,
                                         base::TimeTicks timestamp) = 0;

  virtual void OnFormsSeen(AutofillHandlerProxy* handler,
                           const std::vector<FormData>& forms) = 0;

  virtual void OnHidePopup(AutofillHandlerProxy* handler) = 0;

  virtual void OnServerPredictionsAvailable(AutofillHandlerProxy* handler) = 0;

  virtual void OnServerQueryRequestError(AutofillHandlerProxy* handler,
                                         FormSignature form_signature) = 0;

  virtual void Reset(AutofillHandlerProxy* handler) = 0;

  void SendFormDataToRenderer(AutofillHandlerProxy* handler,
                              int requestId,
                              const FormData& formData);

  // Notifies the renderer should accept the datalist suggestion given by
  // |value| and fill the input field indified by |field_id|.
  void RendererShouldAcceptDataListSuggestion(AutofillHandlerProxy* handler,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& value);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_
