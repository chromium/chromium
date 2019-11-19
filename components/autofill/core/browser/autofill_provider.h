// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_

#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"

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

  virtual void OnFocusNoLongerOnForm(AutofillHandlerProxy* handler) = 0;

  virtual void OnFocusOnFormField(AutofillHandlerProxy* handler,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const gfx::RectF& bounding_box) = 0;

  virtual void OnDidFillAutofillFormData(AutofillHandlerProxy* handler,
                                         const FormData& form,
                                         base::TimeTicks timestamp) = 0;

  virtual void OnFormsSeen(AutofillHandlerProxy* handler,
                           const std::vector<FormData>& forms,
                           const base::TimeTicks timestamp) = 0;

  virtual void Reset(AutofillHandlerProxy* handler) = 0;

  void SendFormDataToRenderer(AutofillHandlerProxy* handler,
                              int requestId,
                              const FormData& formData);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROVIDER_H_
