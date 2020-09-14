// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"

namespace gfx {
class RectF;
}

namespace autofill {

class AutofillField;
struct FormData;
struct FormFieldData;
class FormStructure;
class LogManager;

// This class defines the interface should be implemented by autofill
// implementation in browser side to interact with AutofillDriver.
class AutofillHandler {
 public:
  enum AutofillDownloadManagerState {
    ENABLE_AUTOFILL_DOWNLOAD_MANAGER,
    DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
  };

  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual void OnFormParsed() = 0;
  };

  virtual ~AutofillHandler();

  // Invoked when the value of textfield is changed.
  void OnTextFieldDidChange(const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp);

  // Invoked when the textfield is scrolled.
  void OnTextFieldDidScroll(const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box);

  // Invoked when the value of select is changed.
  void OnSelectControlDidChange(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box);

  // Invoked when the |form| needs to be autofilled, the |bounding_box| is
  // a window relative value of |field|.
  void OnQueryFormFieldAutofill(int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                bool autoselect_first_suggestion);

  // Invoked when |form|'s |field| has focus.
  void OnFocusOnFormField(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box);

  // Invoked when |form| has been submitted.
  // Processes the submitted |form|, saving any new Autofill data to the user's
  // personal profile.
  void OnFormSubmitted(const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source);

  // Invoked when |forms| has been detected.
  void OnFormsSeen(const std::vector<FormData>& forms,
                   const base::TimeTicks timestamp);

  // Invoked when focus is no longer on form.
  virtual void OnFocusNoLongerOnForm() = 0;

  // Invoked when |form| has been filled with the value given by
  // SendFormDataToRenderer.
  virtual void OnDidFillAutofillFormData(const FormData& form,
                                         const base::TimeTicks timestamp) = 0;

  // Invoked when preview autofill value has been shown.
  virtual void OnDidPreviewAutofillFormData() = 0;

  // Invoked when textfeild editing ended
  virtual void OnDidEndTextFieldEditing() = 0;

  // Invoked when popup window should be hidden.
  virtual void OnHidePopup() = 0;

  // Invoked when the options of a select element in the |form| changed.
  virtual void SelectFieldOptionsDidChange(const FormData& form) = 0;

  // Resets cache.
  virtual void Reset();

  // Send the form |data| to renderer for the specified |action|.
  void SendFormDataToRenderer(int query_id,
                              AutofillDriver::RendererFormDataAction action,
                              const FormData& data);

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  bool GetCachedFormAndField(const FormData& form,
                             const FormFieldData& field,
                             FormStructure** form_structure,
                             AutofillField** autofill_field) WARN_UNUSED_RESULT;

  // Returns the number of forms this Autofill handler is aware of.
  size_t NumFormsDetected() const { return form_structures_.size(); }

  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // Returns the present form structures seen by Autofill handler.
  const std::map<FormRendererId, std::unique_ptr<FormStructure>>&
  form_structures() const {
    return form_structures_;
  }

  AutofillDriver* driver() { return driver_; }

#ifdef UNIT_TEST
  // A public wrapper that calls |mutable_form_structures| for testing purposes
  // only.
  std::map<FormRendererId, std::unique_ptr<FormStructure>>*
  mutable_form_structures_for_test() {
    return mutable_form_structures();
  }

  // A public wrapper that calls |ParseForm| for testing purposes only.
  FormStructure* ParseFormForTest(const FormData& form) {
    return ParseForm(form, nullptr);
  }
#endif

 protected:
  AutofillHandler(AutofillDriver* driver, LogManager* log_manager);

  virtual void OnFormSubmittedImpl(const FormData& form,
                                   bool known_success,
                                   mojom::SubmissionSource source) = 0;

  virtual void OnTextFieldDidChangeImpl(const FormData& form,
                                        const FormFieldData& field,
                                        const gfx::RectF& bounding_box,
                                        const base::TimeTicks timestamp) = 0;

  virtual void OnTextFieldDidScrollImpl(const FormData& form,
                                        const FormFieldData& field,
                                        const gfx::RectF& bounding_box) = 0;

  virtual void OnQueryFormFieldAutofillImpl(
      int query_id,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      bool autoselect_first_suggestion) = 0;

  virtual void OnFocusOnFormFieldImpl(const FormData& form,
                                      const FormFieldData& field,
                                      const gfx::RectF& bounding_box) = 0;

  virtual void OnSelectControlDidChangeImpl(const FormData& form,
                                            const FormFieldData& field,
                                            const gfx::RectF& bounding_box) = 0;

  // Return whether the |forms| from OnFormSeen() should be parsed to
  // form_structures.
  virtual bool ShouldParseForms(const std::vector<FormData>& forms,
                                const base::TimeTicks timestamp) = 0;

  // Invoked when forms from OnFormsSeen() has been parsed to |form_structures|.
  virtual void OnFormsParsed(const std::vector<const FormData*>& forms,
                             const base::TimeTicks timestamp) = 0;

  // Returns the number of FormStructures with the given |form_signature| and
  // appends them to |form_structures|. Runs in linear time.
  size_t FindCachedFormsBySignature(
      FormSignature form_signature,
      std::vector<FormStructure*>* form_structures) const;

  // Returns nullptr if no cached form structure is found with a matching
  // |renderer_id|. Runs in logarithmic time.
  FormStructure* FindCachedFormByRendererId(FormRendererId renderer_id) const;

  // Parses the |form| with the server data retrieved from the |cached_form|
  // (if any). Returns nullptr if the form should not be parsed. Otherwise, adds
  // the returned form structure to the |form_structures_|.
  FormStructure* ParseForm(const FormData& form,
                           const FormStructure* cached_form);

  // Returns the page language, if available.
  virtual std::string GetPageLanguage() const;

  bool value_from_dynamic_change_form_ = false;

  std::map<FormRendererId, std::unique_ptr<FormStructure>>*
  mutable_form_structures() {
    return &form_structures_;
  }

 private:
  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* const driver_;

  LogManager* const log_manager_;

  // Our copy of the form data.
  std::map<FormRendererId, std::unique_ptr<FormStructure>> form_structures_;

  // Will be not null only for |SaveCardBubbleViewsFullFormBrowserTest|.
  ObserverForTest* observer_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AutofillHandler);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_H_
