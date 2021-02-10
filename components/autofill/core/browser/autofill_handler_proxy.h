// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_handler.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class AutofillProvider;

// This class forwards AutofillHandler calls to AutofillProvider.
class AutofillHandlerProxy : public AutofillHandler {
 public:
  AutofillHandlerProxy(
      AutofillDriver* driver,
      AutofillClient* client,
      AutofillProvider* provider,
      AutofillHandler::AutofillDownloadManagerState enable_download_manager);
  ~AutofillHandlerProxy() override;

  void OnFocusNoLongerOnForm(bool had_interacted_form) override;

  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;

  void OnDidPreviewAutofillFormData() override {}
  void OnDidEndTextFieldEditing() override {}
  void OnHidePopup() override;
  void SelectFieldOptionsDidChange(const FormData& form) override;

  void Reset() override;

  base::WeakPtr<AutofillHandlerProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool has_server_prediction() const { return has_server_prediction_; }

 protected:
  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;

  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;

  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;

  void OnQueryFormFieldAutofillImpl(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box,
                                    bool autoselect_first_suggestion) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;

  bool ShouldParseForms(const std::vector<FormData>& forms) override;

  void OnBeforeProcessParsedForms() override {}

  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override {}

  void OnAfterProcessParsedForms(
      const DenseSet<FormType>& form_types) override {}

  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;

  void OnServerRequestError(FormSignature form_signature,
                            AutofillDownloadManager::RequestType request_type,
                            int http_error) override;

 private:
  bool has_server_prediction_ = false;
  AutofillProvider* provider_;
  base::WeakPtrFactory<AutofillHandlerProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillHandlerProxy);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_
