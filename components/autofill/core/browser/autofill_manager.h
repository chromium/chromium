// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/translate/core/browser/translate_driver.h"

namespace gfx {
class RectF;
}  // namespace gfx

namespace autofill {

class AutofillField;
class CreditCardAccessManager;
struct FormData;
struct FormFieldData;
class FormStructure;
class LogManager;
class TouchToFillDelegateAndroidImpl;

// This class defines the interface should be implemented by autofill
// implementation in browser side to interact with AutofillDriver.
//
// AutofillManager has two implementations:
// - AndroidAutofillManager for WebView and WebLayer,
// - BrowserAutofillManager for Chrome.
//
// It is owned by the AutofillDriver.
class AutofillManager
    : public AutofillDownloadManager::Observer,
      public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  // Observer of AutofillManager events.
  //
  // OnAfterFoo() is called, perhaps asynchronously (but on the UI thread),
  // after OnBeforeFoo(). The only exceptions where OnBeforeFoo() may be called
  // without a corresponding OnAfterFoo() call are:
  // - if the number of cached forms exceeds `kAutofillManagerMaxFormCacheSize`;
  // - if this AutofillManager has been destroyed or reset in the meantime.
  // - if the request in AutofillDownloadManager was not successful (i.e. no 2XX
  //   response code or a null response body).
  //
  // New pairs of events may be added as needed.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutofillManagerDestroyed(AutofillManager& manager) {}
    virtual void OnAutofillManagerReset(AutofillManager& manager) {}

    virtual void OnBeforeLanguageDetermined(AutofillManager& manager) {}
    virtual void OnAfterLanguageDetermined(AutofillManager& manager) {}

    virtual void OnBeforeFormsSeen(AutofillManager& manager,
                                   base::span<const FormGlobalId> forms) {}
    virtual void OnAfterFormsSeen(AutofillManager& manager,
                                  base::span<const FormGlobalId> forms) {}

    virtual void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field) {}
    virtual void OnAfterTextFieldDidChange(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field) {}

    virtual void OnBeforeDidFillAutofillFormData(AutofillManager& manager,
                                                 FormGlobalId form) {}
    virtual void OnAfterDidFillAutofillFormData(AutofillManager& manager,
                                                FormGlobalId form) {}

    virtual void OnBeforeAskForValuesToFill(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field) {}
    virtual void OnAfterAskForValuesToFill(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field) {}

    virtual void OnBeforeJavaScriptChangedAutofilledValue(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field) {}
    virtual void OnAfterJavaScriptChangedAutofilledValue(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field) {}

    virtual void OnBeforeFormSubmitted(AutofillManager& manager,
                                       FormGlobalId form) {}
    virtual void OnAfterFormSubmitted(AutofillManager& manager,
                                      FormGlobalId form) {}

    virtual void OnBeforeLoadedServerPredictions(AutofillManager& manager) {}
    virtual void OnAfterLoadedServerPredictions(AutofillManager& manager) {}

    // TODO(crbug.com/1330105): Clean up API: delete the events that don't
    // follow the OnBeforeFoo() / OnAfterFoo() pattern.
    virtual void OnFormParsed(AutofillManager& manager) {}
    virtual void OnTextFieldDidChange(AutofillManager& manager) {}
    virtual void OnTextFieldDidScroll(AutofillManager& manager) {}
    virtual void OnSelectControlDidChange(AutofillManager& manager) {}
    virtual void OnFormSubmitted(AutofillManager& manager) {}
  };

  // TODO(crbug.com/1151542): Move to anonymous namespace once
  // BrowserAutofillManager::OnLoadedServerPredictions() moves to
  // AutofillManager.
  static void LogAutofillTypePredictionsAvailable(
      LogManager* log_manager,
      const std::vector<FormStructure*>& forms);

  AutofillManager(const AutofillManager&) = delete;
  AutofillManager& operator=(const AutofillManager&) = delete;

  ~AutofillManager() override;

  // The following will fail a DCHECK if called for a prerendered main frame.
  AutofillClient* client() {
    DCHECK(!driver()->IsPrerendering());
    return client_;
  }

  const AutofillClient* client() const {
    DCHECK(!driver()->IsPrerendering());
    return client_;
  }

  AutofillClient* unsafe_client(
      base::PassKey<TouchToFillDelegateAndroidImpl> pass_key) {
    return AutofillManager::unsafe_client();
  }

  // Returns a WeakPtr to the leaf class.
  virtual base::WeakPtr<AutofillManager> GetWeakPtr() = 0;

  // May return nullptr.
  virtual CreditCardAccessManager* GetCreditCardAccessManager() = 0;

  // Events triggered by the renderer.

  // Returns true only if the previewed form should be cleared.
  virtual bool ShouldClearPreviewedForm() = 0;

  // Invoked when the value of textfield is changed.
  // |bounding_box| are viewport coordinates.
  // Virtual for testing.
  virtual void OnTextFieldDidChange(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box,
                                    const base::TimeTicks timestamp);

  // Invoked when the textfield is scrolled.
  // |bounding_box| are viewport coordinates.
  void OnTextFieldDidScroll(const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box);

  // Invoked when the value of select is changed.
  // |bounding_box| are viewport coordinates.
  void OnSelectControlDidChange(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box);

  // Invoked when the |form| needs to be autofilled, the |bounding_box| is
  // a window relative value of |field|.
  // |bounding_box| are viewport coordinates.
  // Virtual for testing.
  virtual void OnAskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source);

  // Invoked when |form|'s |field| has focus.
  // |bounding_box| are viewport coordinates.
  void OnFocusOnFormField(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box);

  // Invoked when |form| has been submitted.
  // Processes the submitted |form|, saving any new Autofill data to the user's
  // personal profile.
  // Virtual for testing.
  virtual void OnFormSubmitted(const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source);

  void FillCreditCardForm(const FormData& form,
                          const FormFieldData& field,
                          const CreditCard& credit_card,
                          const std::u16string& cvc,
                          const AutofillTriggerSource trigger_source);

  void FillProfileForm(const AutofillProfile& profile,
                       const FormData& form,
                       const FormFieldData& field,
                       const AutofillTriggerSource trigger_source);

  // Invoked when |form| has been filled with the value given by
  // FillOrPreviewForm.
  // Virtual for testing.
  virtual void OnDidFillAutofillFormData(const FormData& form,
                                         const base::TimeTicks timestamp);

  // Invoked when changes of the forms have been detected: the forms in
  // |updated_forms| are either new or have changed, and the forms in
  // |removed_forms| have been removed from the DOM (but may be re-added to the
  // DOM later).
  // Virtual for testing.
  virtual void OnFormsSeen(const std::vector<FormData>& updated_forms,
                           const std::vector<FormGlobalId>& removed_forms);

  // Invoked when focus is no longer on form. |had_interacted_form| indicates
  // whether focus was previously on a form with which the user had interacted.
  void OnFocusNoLongerOnForm(bool had_interacted_form);

  // Invoked when preview autofill value has been shown.
  void OnDidPreviewAutofillFormData();

  // Invoked when textfeild editing ended
  void OnDidEndTextFieldEditing();

  // Invoked when popup window should be hidden.
  void OnHidePopup();

  // Invoked when the options of a select element in the |form| changed.
  void OnSelectFieldOptionsDidChange(const FormData& form);

  // Invoked after JavaScript set the value of |field| in |form|. Only called
  // if |field| was in autofilled state. Note that from a renderer's
  // perspective, modifying the value with JavaScript leads to a state where
  // the field is not considered autofilled anymore. So this notification won't
  // be sent again until the field gets autofilled again.
  virtual void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value);

  // Other events.

  // Invoked when the field type predictions are downloaded from the autofill
  // server.
  virtual void PropagateAutofillPredictions(
      const std::vector<FormStructure*>& forms) = 0;

  virtual void ReportAutofillWebOTPMetrics(bool used_web_otp) = 0;

  // Resets cache.
  virtual void Reset();

  // Invoked when the context menu is opened in a field.
  virtual void OnContextMenuShownInField(
      const FormGlobalId& form_global_id,
      const FieldGlobalId& field_global_id) = 0;

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnTranslateDriverDestroyed(
      translate::TranslateDriver* translate_driver) override;
  // Invoked when the language has been detected by the Translate component.
  // As this usually happens after Autofill has parsed the forms for the first
  // time, the heuristics need to be re-run by this function in order to use
  // language-specific patterns.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  [[nodiscard]] bool GetCachedFormAndField(const FormData& form,
                                           const FormFieldData& field,
                                           FormStructure** form_structure,
                                           AutofillField** autofill_field);

  // Returns nullptr if no cached form structure is found with a matching
  // |form_id|. Runs in logarithmic time.
  FormStructure* FindCachedFormById(FormGlobalId form_id) const;

  // Returns the number of forms this Autofill handler is aware of.
  size_t NumFormsDetected() const { return form_structures_.size(); }

  // Forwards call to the same-named `AutofillDriver` function.
  virtual void SetShouldSuppressKeyboard(bool suppress);

  // Forwards call to the same-named `AutofillDriver` function.
  virtual bool CanShowAutofillUi() const;

  // Forwards call to the same-named `AutofillDriver` function.
  virtual void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool success)> form_extraction_finished_callback);

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  template <typename Functor, typename... Args>
  void NotifyObservers(const Functor& functor, const Args&... args) {
    for (Observer& observer : observers_) {
      base::invoke(functor, observer, *this, args...);
    }
  }

  // Returns the present form structures seen by Autofill handler.
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>&
  form_structures() const {
    return form_structures_;
  }

  AutofillDriver* driver() { return driver_; }
  const AutofillDriver* driver() const { return driver_; }

  AutofillDownloadManager* download_manager() {
    return client()->GetDownloadManager();
  }

  // The return value shouldn't be cached, retrieve it as needed.
  AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger() {
    return form_interactions_ukm_logger_.get();
  }

  // A public wrapper that calls |OnLoadedServerPredictions| for testing
  // purposes only, it is used by WebView integration test and unit test, so it
  // can't be in #ifdef UNIT_TEST.
  void OnLoadedServerPredictionsForTest(
      std::string response,
      const std::vector<FormSignature>& queried_form_signatures) {
    OnLoadedServerPredictions(response, queried_form_signatures);
  }

  std::map<FormGlobalId, std::unique_ptr<FormStructure>>*
  mutable_form_structures_for_test() {
    return mutable_form_structures();
  }

  FormStructure* ParseFormForTest(const FormData& form) {
    return ParseForm(form, nullptr);
  }

 protected:
  AutofillManager(AutofillDriver* driver, AutofillClient* client);

  LogManager* log_manager() { return log_manager_; }

  // Retrieves the page language from |client_|
  LanguageCode GetCurrentPageLanguage();

  // The following do not check for prerendering. These should only used while
  // constructing or resetting the manager.
  // TODO(crbug.com/1239281): if we never intend to support multiple navigations
  // while prerendering, these will be unnecessary (they're used during Reset
  // which can be called during prerendering, but we could skip Reset for
  // prerendering if we never have state to clear).
  AutofillClient* unsafe_client() { return client_; }
  const AutofillClient* unsafe_client() const { return client_; }

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

  virtual void OnAskForValuesToFillImpl(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) = 0;

  virtual void OnFocusOnFormFieldImpl(const FormData& form,
                                      const FormFieldData& field,
                                      const gfx::RectF& bounding_box) = 0;

  virtual void OnSelectControlDidChangeImpl(const FormData& form,
                                            const FormFieldData& field,
                                            const gfx::RectF& bounding_box) = 0;

  virtual void OnDidFillAutofillFormDataImpl(
      const FormData& form,
      const base::TimeTicks timestamp) = 0;

  virtual void FillCreditCardFormImpl(const FormData& form,
                                      const FormFieldData& field,
                                      const CreditCard& credit_card,
                                      const std::u16string& cvc,
                                      AutofillTriggerSource trigger_source) = 0;
  virtual void FillProfileFormImpl(const FormData& form,
                                   const FormFieldData& field,
                                   const AutofillProfile& profile,
                                   AutofillTriggerSource trigger_source) = 0;

  virtual void OnFocusNoLongerOnFormImpl(bool had_interacted_form) = 0;

  virtual void OnDidPreviewAutofillFormDataImpl() = 0;

  virtual void OnDidEndTextFieldEditingImpl() = 0;

  virtual void OnHidePopupImpl() = 0;

  virtual void OnSelectFieldOptionsDidChangeImpl(const FormData& form) = 0;

  virtual void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) = 0;

  // Return whether the |forms| from OnFormSeen() should be parsed to
  // form_structures.
  virtual bool ShouldParseForms() = 0;

  // Invoked before parsing the forms.
  // TODO(crbug.com/1309848): Rename to some consistent scheme, e.g.,
  // OnBeforeParsedForm().
  virtual void OnBeforeProcessParsedForms() = 0;

  // Invoked when the given |form| has been processed to the given
  // |form_structure|.
  virtual void OnFormProcessed(const FormData& form_data,
                               const FormStructure& form_structure) = 0;
  // Invoked after all forms have been processed, |form_types| is a set of
  // FormType found.
  virtual void OnAfterProcessParsedForms(
      const DenseSet<FormType>& form_types) = 0;

  // Returns the number of FormStructures with the given |form_signature| and
  // appends them to |form_structures|. Runs in linear time.
  size_t FindCachedFormsBySignature(
      FormSignature form_signature,
      std::vector<FormStructure*>* form_structures) const;

  // Parses multiple forms in one go. The function proceeds in three stages:
  //
  // 1. Turn (almost) every FormData into a FormStructure.
  // 2. Run DetermineHeuristicTypes() on all FormStructures.
  // 3. Update the cache member variable `form_structures_` and call `callback`.
  //
  // Step 1 runs synchronously on the main thread.
  // Step 2 runs asynchronously on a worker task.
  // Step 3 runs again on the main thread.
  //
  // There are two conditions under which a FormData is skipped in Step 1:
  // - if the overall number exceeds `kAutofillManagerMaxFormCacheSize`;
  // - if the form should not be parsed according to ShouldParseForms().
  //
  // TODO(crbug.com/1309848): Add unit tests.
  // TODO(crbug.com/1345089): Eliminate either the ParseFormsAsync() or
  // ParseFormAsync(). There are a few possible directions:
  // - Let ParseFormAync() wrap the FormData in a vector, call
  //   ParseFormsAsync(), and then unwrap the vector again.
  // - Let OnFormsSeen() take a single FormData. That simplifies also
  //   ContentAutofillDriver and ContentAutofillRouter a bit, but then the
  //   AutofillDownloadManager needs to collect forms to send a batch query.
  // - Let all other events take a FormGlobalId instead of a FormData and fire
  //   OnFormsSeen() before these events if necessary.
  void ParseFormsAsync(
      const std::vector<FormData>& forms,
      base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
          callback);

  // Parses a single form analogously to ParseFormsAsync().
  void ParseFormAsync(
      const FormData& form,
      base::OnceCallback<void(AutofillManager&, const FormData&)> callback);

  // Parses the |form| with the server data retrieved from the |cached_form|
  // (if any). Returns nullptr if the form should not be parsed. Otherwise, adds
  // the returned form structure to the |form_structures_|.
  FormStructure* ParseForm(const FormData& form,
                           const FormStructure* cached_form);

  bool value_from_dynamic_change_form_ = false;

  std::map<FormGlobalId, std::unique_ptr<FormStructure>>*
  mutable_form_structures() {
    return &form_structures_;
  }

 private:
  // AutofillDownloadManager::Observer:
  void OnLoadedServerPredictions(
      std::string response,
      const std::vector<FormSignature>& queried_form_signatures) override;

  // Invoked when forms from OnFormsSeen() have been parsed to
  // |form_structures|.
  void OnFormsParsed(const std::vector<FormData>& forms);

  std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
  CreateFormInteractionsUkmLogger();

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  const raw_ptr<AutofillDriver, DanglingUntriaged> driver_;

  // Do not access this directly. Instead, please use client() or
  // unsafe_client(). These functions check (or explicitly don't check) that the
  // client isn't accessed incorrectly.
  const raw_ptr<AutofillClient> client_;

  const raw_ptr<LogManager> log_manager_;

  // Observer needed to re-run heuristics when the language has been detected.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  // Our copy of the form data.
  std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures_;

  // Utility for logging URL keyed metrics.
  std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Observers that listen to updates of this instance.
  base::ObserverList<Observer> observers_;

  // DetermineHeuristicTypes() should only be run on the `parsing_task_runner_`.
  // The reply will be called on the main thread and should be a no-op if this
  // AutofillManager has been destroyed or reset; to detect this, the reply
  // should take a WeakPtr from `parsing_weak_ptr_factory_`.
  scoped_refptr<base::SequencedTaskRunner> parsing_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE});
  base::WeakPtrFactory<AutofillManager> parsing_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
