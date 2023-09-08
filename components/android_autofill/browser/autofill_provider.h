// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace autofill {

class AndroidAutofillManager;

// This class defines the interface for the autofill implementation other than
// default BrowserAutofillManager. Unlike BrowserAutofillManager, this class
// has one instance per WebContents.
class AutofillProvider : public content::WebContentsUserData<AutofillProvider> {
 public:
  ~AutofillProvider() override;

  static bool is_download_manager_disabled_for_testing();
  static void set_is_download_manager_disabled_for_testing();

  virtual void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) = 0;

  virtual void OnTextFieldDidChange(AndroidAutofillManager* manager,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box,
                                    const base::TimeTicks timestamp) = 0;

  virtual void OnTextFieldDidScroll(AndroidAutofillManager* manager,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) = 0;

  virtual void OnSelectControlDidChange(AndroidAutofillManager* manager,
                                        const FormData& form,
                                        const FormFieldData& field,
                                        const gfx::RectF& bounding_box) = 0;

  virtual void OnFormSubmitted(AndroidAutofillManager* manager,
                               const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source) = 0;

  virtual void OnFocusNoLongerOnForm(AndroidAutofillManager* manager,
                                     bool had_interacted_form) = 0;

  virtual void OnFocusOnFormField(AndroidAutofillManager* manager,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const gfx::RectF& bounding_box) = 0;

  virtual void OnDidFillAutofillFormData(AndroidAutofillManager* manager,
                                         const FormData& form,
                                         base::TimeTicks timestamp) = 0;

  virtual void OnHidePopup(AndroidAutofillManager* manager) = 0;

  virtual void OnServerPredictionsAvailable(AndroidAutofillManager* manager,
                                            FormGlobalId form) = 0;

  virtual void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                         FormSignature form_signature) = 0;

  virtual void Reset(AndroidAutofillManager* manager) = 0;

  // Returns autofilled state from AutofillProvider's cache.
  virtual bool GetCachedIsAutofilled(const FormFieldData& field) const = 0;

  void FillOrPreviewForm(AndroidAutofillManager* manager,
                         const FormData& form_data,
                         FieldTypeGroup field_type_group,
                         const url::Origin& triggered_origin);

  // Notifies the renderer should accept the datalist suggestion given by
  // |value| and fill the input field indified by |field_id|.
  void RendererShouldAcceptDataListSuggestion(AndroidAutofillManager* manager,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& value);

 protected:
  // WebContents takes the ownership of AutofillProvider.
  explicit AutofillProvider(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutofillProvider>;

 private:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_H_
