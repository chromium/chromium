// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/is_required.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/translate/core/browser/translate_driver.h"

namespace autofill {

class AutofillField;
class AutofillProfile;
class CreditCard;
class FormData;
class FormFieldData;
class FormStructure;
class LogManager;

// This class defines the interface should be implemented by autofill
// implementation in browser side to interact with AutofillDriver.
//
// AutofillManager has two implementations:
// - AndroidAutofillManager for WebView,
// - BrowserAutofillManager for Chrome.
//
// It is owned by the AutofillDriver.
class AutofillManager
    : public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  using LifecycleState = AutofillDriver::LifecycleState;

  // Observer of AutofillManager events.
  //
  // For the On{Before,After}Foo() events, the following invariant holds:
  // Every OnBeforeFoo() is followed by an OnAfterFoo(); on OnAfterFoo() may be
  // called asynchronously (but on the UI thread). The only exceptions where
  // OnBeforeFoo() may be called without a corresponding OnAfterFoo() call are:
  // - if the number of cached forms exceeds `kAutofillManagerMaxFormCacheSize`;
  // - if this AutofillManager has been destroyed or reset in the meantime.
  //
  // When observing an AutofillManager, make sure to remove the observation
  // before the AutofillManager is destroyed. Pending destruction is signaled
  // by a call to `OnAutofillManagerStateChanged` with current `LifecycleState`
  // `kPendingDeletion`.
  // If you want to observe all AutofillManagers of a `WebContents`, consider
  // using `autofill::ScopedAutofillManagersObservation`, which abstracts away
  // all the boilerplate for adding and removing observers of AutofillManagers
  // of a `WebContents`.
  //
  // TODO(crbug.com/40280003): Consider moving events that are specific to BAM
  // to a new BAM::Observer class.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutofillManagerStateChanged(AutofillManager& manager,
                                               LifecycleState previous,
                                               LifecycleState current) {}

    virtual void OnBeforeLanguageDetermined(AutofillManager& manager) {}
    virtual void OnAfterLanguageDetermined(AutofillManager& manager) {}

    virtual void OnBeforeFormsSeen(
        AutofillManager& manager,
        base::span<const FormGlobalId> updated_forms,
        base::span<const FormGlobalId> removed_forms) {}
    virtual void OnAfterFormsSeen(
        AutofillManager& manager,
        base::span<const FormGlobalId> updated_forms,
        base::span<const FormGlobalId> removed_forms) {}

    virtual void OnBeforeCaretMovedInFormField(AutofillManager& manager,
                                               const FormGlobalId& form,
                                               const FieldGlobalId& field_id,
                                               const std::u16string& selection,
                                               const gfx::Rect& caret_bounds) {}
    virtual void OnAfterCaretMovedInFormField(AutofillManager& manager,
                                              const FormGlobalId& form,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& selection,
                                              const gfx::Rect& caret_bounds) {}

    virtual void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field) {}

    // TODO(crbug.com/40227496): Get rid of `text_value`.
    virtual void OnAfterTextFieldDidChange(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field,
                                           const std::u16string& text_value) {}

    virtual void OnBeforeTextFieldDidScroll(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field) {}
    virtual void OnAfterTextFieldDidScroll(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field) {}

    virtual void OnBeforeSelectControlDidChange(AutofillManager& manager,
                                                FormGlobalId form,
                                                FieldGlobalId field) {}
    virtual void OnAfterSelectControlDidChange(AutofillManager& manager,
                                               FormGlobalId form,
                                               FieldGlobalId field) {}

    virtual void OnBeforeAskForValuesToFill(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field,
                                            const FormData& form_data) {}
    virtual void OnAfterAskForValuesToFill(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field) {}

    virtual void OnBeforeFocusOnFormField(AutofillManager& manager,
                                          FormGlobalId form,
                                          FieldGlobalId field) {}
    virtual void OnAfterFocusOnFormField(AutofillManager& manager,
                                         FormGlobalId form,
                                         FieldGlobalId field) {}

    virtual void OnBeforeDidFillAutofillFormData(AutofillManager& manager,
                                                 FormGlobalId form) {}
    virtual void OnAfterDidFillAutofillFormData(AutofillManager& manager,
                                                FormGlobalId form) {}

    virtual void OnBeforeJavaScriptChangedAutofilledValue(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field) {}
    virtual void OnAfterJavaScriptChangedAutofilledValue(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field) {}

    virtual void OnBeforeLoadedServerPredictions(AutofillManager& manager) {}
    virtual void OnAfterLoadedServerPredictions(AutofillManager& manager) {}

    // Fired when the field types predictions of a form *may* have changed.
    // At the moment, we cannot distinguish whether autocomplete attributes or
    // local heuristics changed.
    enum class FieldTypeSource { kHeuristicsOrAutocomplete, kAutofillServer };
    virtual void OnFieldTypesDetermined(AutofillManager& manager,
                                        FormGlobalId form,
                                        FieldTypeSource source) {}

    // Fired when the suggestions are *actually* shown or hidden.
    virtual void OnSuggestionsShown(AutofillManager& manager) {}
    virtual void OnSuggestionsHidden(AutofillManager& manager) {}

    // Fired when a form is filled or previewed with a AutofillProfile or
    // CreditCard.
    // `filled_fields` represents the fields that were sent to the renderer to
    // be filled: each `FormFieldData::value` contains the filled or previewed
    // value; the corresponding `AutofillField` contains the field type
    // information. The field values come from `profile_or_credit_card`.
    // TODO(crbug.com/40227496): Get rid of FormFieldData.
    // TODO(crbug.com/40280003): Consider removing the event in favor of
    // OnAfterDidFillAutofillFormData(), which is fired by the renderer.
    virtual void OnFillOrPreviewDataModelForm(
        AutofillManager& manager,
        FormGlobalId form,
        mojom::ActionPersistence action_persistence,
        base::span<const FormFieldData* const> filled_fields,
        absl::variant<const AutofillProfile*, const CreditCard*>
            profile_or_credit_card) {}

    // Fired when a form is submitted. A `FormData` is passed instead of a
    // `FormGlobalId` because the form structure cached inside `AutofillManager`
    // is not updated at this point yet and thus does not contain, e.g., the
    // submitted values, that an observer may wish to analyze.
    virtual void OnFormSubmitted(AutofillManager& manager,
                                 const FormData& form) {}
  };

  // TODO(crbug.com/40733066): Move to anonymous namespace once
  // BrowserAutofillManager::OnLoadedServerPredictions() moves to
  // AutofillManager.
  static void LogTypePredictionsAvailable(
      LogManager* log_manager,
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms);

  AutofillManager(const AutofillManager&) = delete;
  AutofillManager& operator=(const AutofillManager&) = delete;

  ~AutofillManager() override;

  // Notifies `Observer`s and calls Reset() if applicable.
  void OnAutofillDriverLifecycleStateChanged(
      LifecycleState old_state,
      LifecycleState new_state,
      base::PassKey<AutofillDriverFactory> pass_key);

  AutofillClient& client() { return driver_->GetAutofillClient(); }
  const AutofillClient& client() const { return driver_->GetAutofillClient(); }

  // Returns a WeakPtr to the leaf class.
  virtual base::WeakPtr<AutofillManager> GetWeakPtr() = 0;

  // Events triggered by the renderer.
  // See autofill_driver.mojom for documentation.
  // Some functions are virtual for testing.
  virtual void OnFormsSeen(const std::vector<FormData>& updated_forms,
                           const std::vector<FormGlobalId>& removed_forms);
  virtual void OnFormSubmitted(const FormData& form,
                               bool known_success,
                               mojom::SubmissionSource source);
  virtual void OnTextFieldDidChange(const FormData& form,
                                    const FieldGlobalId& field_id,
                                    const base::TimeTicks timestamp);
  void OnDidEndTextFieldEditing();
  virtual void OnTextFieldDidScroll(const FormData& form,
                                    const FieldGlobalId& field_id);
  virtual void OnSelectControlDidChange(const FormData& form,
                                        const FieldGlobalId& field_id);
  void OnSelectFieldOptionsDidChange(const FormData& form);
  virtual void OnFocusOnFormField(const FormData& form,
                                  const FieldGlobalId& field_id);
  void OnFocusOnNonFormField();
  virtual void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source);
  void OnHidePopup();
  virtual void OnCaretMovedInFormField(const FormData& form,
                                       const FieldGlobalId& field_id,
                                       const gfx::Rect& caret_bounds);
  virtual void OnDidFillAutofillFormData(const FormData& form,
                                         const base::TimeTicks timestamp);
  virtual void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value,
      bool formatting_only);

  // Invoked when the suggestions are actually hidden.
  void OnSuggestionsHidden();

  // Other events.

  virtual void ReportAutofillWebOTPMetrics(bool used_web_otp) = 0;

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnTranslateDriverDestroyed(
      translate::TranslateDriver* translate_driver) override;
  // Invoked when the language has been detected by the Translate component.
  // As this usually happens after Autofill has parsed the forms for the first
  // time, the heuristics need to be re-run by this function in order to use
  // language-specific patterns. Since the ML model doesn't depend on the page
  // language, its predictions are not recomputed.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  [[nodiscard]] bool GetCachedFormAndField(
      const FormData& form,
      const FormFieldData& field,
      FormStructure** form_structure,
      AutofillField** autofill_field) const;

  // Returns nullptr if no cached form structure is found with a matching
  // |form_id|. Runs in logarithmic time.
  FormStructure* FindCachedFormById(FormGlobalId form_id) const;

  // Returns the number of forms this Autofill handler is aware of.
  size_t NumFormsDetected() const { return form_structures_.size(); }

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
      std::invoke(functor, observer, *this, args...);
    }
  }

  // Returns the present form structures seen by Autofill handler.
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>&
  form_structures() const {
    return form_structures_;
  }

  AutofillDriver& driver() { return *driver_; }

  // The return value shouldn't be cached, retrieve it as needed.
  AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger() {
    return form_interactions_ukm_logger_.get();
  }

 protected:
  explicit AutofillManager(AutofillDriver* driver);

  // Clears the managed forms and other objects held by the AutofillManager.
  // Does not touch the LifecycleState, which is controlled by the caller.
  virtual void Reset();

  LogManager* log_manager() { return log_manager_; }

  // Retrieves the page language from |client_|
  LanguageCode GetCurrentPageLanguage();

  // OnFooImpl() is called, potentially asynchronously after parsing the form,
  // by the renderer event OnFoo().
  virtual void OnFormSubmittedImpl(const FormData& form,
                                   bool known_success,
                                   mojom::SubmissionSource source) = 0;
  virtual void OnCaretMovedInFormFieldImpl(const FormData& form,
                                           const FieldGlobalId& field_id,
                                           const gfx::Rect& caret_bounds) = 0;
  virtual void OnTextFieldDidChangeImpl(const FormData& form,
                                        const FieldGlobalId& field_id,
                                        const base::TimeTicks timestamp) = 0;
  virtual void OnDidEndTextFieldEditingImpl() = 0;
  virtual void OnTextFieldDidScrollImpl(const FormData& form,
                                        const FieldGlobalId& field_id) = 0;
  virtual void OnSelectControlDidChangeImpl(const FormData& form,
                                            const FieldGlobalId& field_id) = 0;
  virtual void OnSelectFieldOptionsDidChangeImpl(const FormData& form) = 0;
  virtual void OnFocusOnFormFieldImpl(const FormData& form,
                                      const FieldGlobalId& field_id) = 0;
  virtual void OnFocusOnNonFormFieldImpl() = 0;
  virtual void OnAskForValuesToFillImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) = 0;
  virtual void OnDidFillAutofillFormDataImpl(
      const FormData& form,
      const base::TimeTicks timestamp) = 0;
  virtual void OnHidePopupImpl() = 0;
  virtual void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value,
      bool formatting_only) = 0;

  // Return whether the |forms| from OnFormSeen() should be parsed to
  // form_structures.
  virtual bool ShouldParseForms() = 0;

  // Invoked before parsing the forms.
  // TODO(crbug.com/40219607): Rename to some consistent scheme, e.g.,
  // OnBeforeParsedForm().
  virtual void OnBeforeProcessParsedForms() = 0;

  // Invoked when the given |form| has been processed to the given
  // |form_structure|.
  virtual void OnFormProcessed(const FormData& form_data,
                               const FormStructure& form_structure) = 0;

  // Returns the number of FormStructures with the given |form_signature| and
  // appends them to |form_structures|. Runs in linear time.
  size_t FindCachedFormsBySignature(
      FormSignature form_signature,
      std::vector<raw_ptr<FormStructure, VectorExperimental>>* form_structures)
      const;

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
  // TODO(crbug.com/40219607): Add unit tests.
  // TODO(crbug.com/40232021): Eliminate either the ParseFormsAsync() or
  // ParseFormAsync(). There are a few possible directions:
  // - Let ParseFormAsync() wrap the FormData in a vector, call
  //   ParseFormsAsync(), and then unwrap the vector again.
  // - Let OnFormsSeen() take a single FormData. That simplifies also
  //   ContentAutofillDriver and AutofillDriverRouter a bit, but then the
  //   AutofillCrowdsourcingManager needs to collect forms to send a batch
  //   query.
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

  // Returns true only if the previewed form should be cleared.
  virtual bool ShouldClearPreviewedForm() = 0;

  std::map<FormGlobalId, std::unique_ptr<FormStructure>>*
  mutable_form_structures() {
    return &form_structures_;
  }

 private:
  friend class AutofillManagerTestApi;

  // Invoked by `AutofillCrowdsourcingManager`.
  void OnLoadedServerPredictions(
      std::optional<AutofillCrowdsourcingManager::QueryResponse> response);

  // Invoked when forms from OnFormsSeen() have been parsed to
  // |form_structures|.
  void OnFormsParsed(const std::vector<FormData>& forms);

  std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
  CreateFormInteractionsUkmLogger();

  // Returns a callback that runs `callback` on the main thread after all
  // ongoing async parsing operations have finished.
  template <typename... Args>
  base::OnceCallback<void(Args...)> AfterParsingFinishes(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(
        [](base::WeakPtr<AutofillManager> self,
           base::OnceCallback<void(Args...)> callback, Args... args) {
          if (self) {
            self->parsing_task_runner_->PostTaskAndReply(
                FROM_HERE, base::DoNothing(),
                base::BindOnce(std::move(callback),
                               std::forward<Args>(args)...));
          }
        },
        GetWeakPtr(), std::move(callback));
  }

  // Provides driver-level context to the shared code of the component.
  // `*driver_` owns this object.
  const raw_ref<AutofillDriver> driver_;

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
