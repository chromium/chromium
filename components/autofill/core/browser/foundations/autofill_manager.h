// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/callback_list.h"
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
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
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

struct AutofillServerPrediction;
class AutofillField;
class AutofillProfile;
class CreditCard;
class CreditCardAccessManager;
class FormData;
class FormFieldData;
class FormStructure;
class LogManager;
struct Suggestion;

namespace autofill_metrics {
class FormInteractionsUkmLogger;
}

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

    virtual void OnBeforeTextFieldValueChanged(AutofillManager& manager,
                                               FormGlobalId form,
                                               FieldGlobalId field) {}

    // TODO(crbug.com/40227496): Get rid of `text_value`.
    virtual void OnAfterTextFieldValueChanged(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field,
        const std::u16string& text_value) {}

    virtual void OnBeforeTextFieldDidScroll(AutofillManager& manager,
                                            FormGlobalId form,
                                            FieldGlobalId field) {}
    virtual void OnAfterTextFieldDidScroll(AutofillManager& manager,
                                           FormGlobalId form,
                                           FieldGlobalId field) {}

    virtual void OnBeforeSelectControlSelectionChanged(AutofillManager& manager,
                                                       FormGlobalId form,
                                                       FieldGlobalId field) {}
    virtual void OnAfterSelectControlSelectionChanged(AutofillManager& manager,
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

    virtual void OnBeforeFocusOnNonFormField(AutofillManager& manager) {}
    virtual void OnAfterFocusOnNonFormField(AutofillManager& manager) {}

    virtual void OnBeforeSelectFieldOptionsDidChange(AutofillManager& manager,
                                                     FormGlobalId form) {}
    virtual void OnAfterSelectFieldOptionsDidChange(AutofillManager& manager,
                                                    FormGlobalId form) {}

    virtual void OnBeforeDidAutofillForm(AutofillManager& manager,
                                         FormGlobalId form) {}
    virtual void OnAfterDidAutofillForm(AutofillManager& manager,
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
    enum class FieldTypeSource {
      kHeuristicsOrAutocomplete,
      kAutofillServer,
      kAutofillAiModel
    };
    virtual void OnFieldTypesDetermined(AutofillManager& manager,
                                        FormGlobalId form,
                                        FieldTypeSource source) {}

    // Fired when the suggestions are *actually* shown or hidden.
    virtual void OnSuggestionsShown(AutofillManager& manager,
                                    base::span<const Suggestion> suggestions) {}
    virtual void OnSuggestionsHidden(AutofillManager& manager) {}

    // Fired when a form is filled or previewed with a AutofillProfile or
    // CreditCard.
    // `filled_fields` represents the fields that were sent to the renderer to
    // be filled: each `FormFieldData::value` contains the filled or previewed
    // value; the corresponding `AutofillField` contains the field type
    // information. The field values come from `filling_payload`.
    // TODO(crbug.com/40280003): Consider removing the event in favor of
    // OnAfterDidAutofillForm(), which is fired by the renderer.
    // TODO(crbug.com/40227071): Consider removing `action_persistence` as the
    // preview signal is only used for testing.
    virtual void OnFillOrPreviewForm(
        AutofillManager& manager,
        FormGlobalId form_id,
        mojom::ActionPersistence action_persistence,
        const base::flat_set<FieldGlobalId>& filled_field_ids,
        const FillingPayload& filling_payload) {}

    // Fired when a form is submitted. A `FormData` is passed instead of a
    // `FormGlobalId` because the form structure cached inside `AutofillManager`
    // is not updated at this point yet and thus does not contain, e.g., the
    // submitted values, that an observer may wish to analyze.
    virtual void OnBeforeFormSubmitted(AutofillManager& manager,
                                       const FormData& form) {}
    virtual void OnAfterFormSubmitted(AutofillManager& manager,
                                      const FormData& form) {}
  };

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
                               mojom::SubmissionSource source);
  virtual void OnTextFieldValueChanged(const FormData& form,
                                       const FieldGlobalId& field_id,
                                       const base::TimeTicks timestamp);
  virtual void OnDidEndTextFieldEditing();
  virtual void OnTextFieldDidScroll(const FormData& form,
                                    const FieldGlobalId& field_id);
  virtual void OnSelectControlSelectionChanged(const FormData& form,
                                               const FieldGlobalId& field_id);
  virtual void OnSelectFieldOptionsDidChange(const FormData& form,
                                             const FieldGlobalId& field_id);
  virtual void OnFocusOnFormField(const FormData& form,
                                  const FieldGlobalId& field_id);
  void OnFocusOnNonFormField();
  virtual void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<PasswordSuggestionRequest> password_request);
  void OnHidePopup();
  virtual void OnCaretMovedInFormField(const FormData& form,
                                       const FieldGlobalId& field_id,
                                       const gfx::Rect& caret_bounds);
  virtual void OnDidAutofillForm(const FormData& form);
  virtual void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value);

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

  // Fills `form_structure` and `autofill_field` with the cached elements
  // corresponding to `form_id` and `field_id`.  This might have the side-effect
  // of updating the cache.  Returns false if the form is not autofillable, or
  // if either the form or the field cannot be found.
  [[nodiscard]] bool GetCachedFormAndField(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      FormStructure** form_structure,
      AutofillField** autofill_field) const;

  // Returns nullptr if no cached form structure is found with a matching
  // `form_id`. Runs in logarithmic time.
  FormStructure* FindCachedFormById(FormGlobalId form_id) const;

  // Searches for any cached form that contains a field with `field_id`.
  FormStructure* FindCachedFormById(FieldGlobalId field_id) const;

  // Returns the number of forms this Autofill handler is aware of.
  size_t NumFormsDetected() const { return form_structures_.size(); }

  // Forwards call to the same-named `AutofillDriver` function.
  virtual bool CanShowAutofillUi() const;

  // Forwards call to the same-named `AutofillDriver` function.
  virtual void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool success)> form_extraction_finished_callback);

  // Returns server predictions for fields identified by `field_ids` in a form
  // identified by `form_id`. If the manager has no data about the form with
  // `form_id`, returns an empty map. If the form does not contain data about
  // fields with `field_ids`, NO_SERVER_DATA type is returned for them.
  base::flat_map<FieldGlobalId, AutofillServerPrediction>
  GetServerPredictionsForForm(
      FormGlobalId form_id,
      const std::vector<FieldGlobalId>& field_ids) const;

  // Returns predictions from a heuristic source for fields identified by
  // `field_ids` in a form identified by `form_id`. Returns an empty map if the
  // manager has no data about the form.
  base::flat_map<FieldGlobalId, FieldType> GetHeuristicPredictionForForm(
      HeuristicSource source,
      FormGlobalId form_id,
      const std::vector<FieldGlobalId>& field_ids) const;

  // Returns the `CreditCardAccessManager` associated with `this`. Null only
  // for Android (i.e., platform) Autofill.
  virtual CreditCardAccessManager* GetCreditCardAccessManager() = 0;
  virtual const CreditCardAccessManager* GetCreditCardAccessManager() const = 0;

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

  // Reparses all known forms.
  void ReparseKnownForms();

  // After subscribing, FieldClassificationModelHandler::OnModelUpdated() will
  // trigger ReparseKnownForms(). There may be a handler for Autofill and/or
  // Password Manager.
  void SubscribeToMlModelChanges(FieldClassificationModelHandler& handler);

 protected:
  explicit AutofillManager(AutofillDriver* driver);

  // Clears the managed forms and other objects held by the AutofillManager.
  // Does not touch the LifecycleState, which is controlled by the caller.
  virtual void Reset();

  LogManager* log_manager() { return client().GetCurrentLogManager(); }

  // Retrieves the page language from |client_|
  LanguageCode GetCurrentPageLanguage();

  // OnFooImpl() is called, potentially asynchronously after parsing the form,
  // by the renderer event OnFoo().
  virtual void OnFormSubmittedImpl(const FormData& form,
                                   mojom::SubmissionSource source) = 0;
  virtual void OnCaretMovedInFormFieldImpl(const FormData& form,
                                           const FieldGlobalId& field_id,
                                           const gfx::Rect& caret_bounds) = 0;
  virtual void OnTextFieldValueChangedImpl(const FormData& form,
                                           const FieldGlobalId& field_id,
                                           const base::TimeTicks timestamp) = 0;
  virtual void OnDidEndTextFieldEditingImpl() = 0;
  virtual void OnTextFieldDidScrollImpl(const FormData& form,
                                        const FieldGlobalId& field_id) = 0;
  virtual void OnSelectControlSelectionChangedImpl(
      const FormData& form,
      const FieldGlobalId& field_id) = 0;
  virtual void OnSelectFieldOptionsDidChangeImpl(
      const FormData& form,
      const FieldGlobalId& field_id) = 0;
  virtual void OnFocusOnFormFieldImpl(const FormData& form,
                                      const FieldGlobalId& field_id) = 0;
  virtual void OnFocusOnNonFormFieldImpl() = 0;
  virtual void OnAskForValuesToFillImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<PasswordSuggestionRequest> password_request) = 0;
  virtual void OnDidAutofillFormImpl(const FormData& form) = 0;
  virtual void OnHidePopupImpl() = 0;
  virtual void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value) = 0;
  virtual void OnLoadedServerPredictionsImpl(
      base::span<const raw_ptr<FormStructure, VectorExperimental>> forms) = 0;

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

  // Returns true only if the previewed form should be cleared.
  virtual bool ShouldClearPreviewedForm() = 0;

  // Logs the field types of `form` to chrome://autofill-internals and the
  // autofill-information attribute (if
  // `features::debug::kAutofillShowTypePredictions` is enabled).
  void LogCurrentFieldTypes(
      std::variant<const FormData*, const FormStructure*> form);

 private:
  friend class AutofillManagerTestApi;

  struct AsyncContext;

  // Parses multiple forms in one go. The function proceeds in four stages:
  //
  // 1. Turn (almost) every FormData into a FormStructure.
  // 2. Runs ML models on all FormStructures, if the necessary features are
  //    enabled.
  // 3. Run DetermineHeuristicTypes() on all FormStructures.
  // 4. Update the cache member variable `form_structures_` and call `callback`.
  //
  // Step 1 runs synchronously on the main thread.
  // Step 2 and 3 run sequentially, but asynchronously on (different) worker
  // tasks.
  // Step 4 runs again on the main thread.
  //
  // There are two conditions under which a FormData is skipped in Step 1:
  // - if the overall number exceeds `kAutofillManagerMaxFormCacheSize`;
  // - if the form should not be parsed according to ShouldParseForms().
  //
  // TODO(crbug.com/40219607): Add unit tests.
  void ParseFormsAsync(
      const std::vector<FormData>& forms,
      base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
          callback);

  // Parses a single form analogously to ParseFormsAsync().
  void ParseFormAsync(
      const FormData& form,
      base::OnceCallback<void(AutofillManager&, const FormData&)> callback);

  // Steps 2-4 described above ParseFormsAsync(), which are shared with
  // ParseFormAsync().
  void ParseFormsAsyncCommon(
      bool preserve_signatures,
      std::vector<FormData> forms,
      base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
          callback);

  // Step 2 described above ParseFormsAsync().
  void RunMlModels(AsyncContext context,
                   base::OnceCallback<void(AsyncContext)> done_callback);

  // Invoked by `AutofillCrowdsourcingManager`.
  void OnLoadedServerPredictions(
      std::optional<AutofillCrowdsourcingManager::QueryResponse> response);

  // Invoked when forms from OnFormsSeen() have been parsed to
  // |form_structures|.
  void OnFormsParsed(const std::vector<FormData>& forms);

  // Updates `form_structures_` with the information in `forms` and `context`,
  // if available. `context` is available when this function is called as a
  // result of a parsing operation, `reason` is an indicator of that.
  // If `preserve_signatures` is true, credit card forms have their
  // `FormSignature`s preserved. `forms` might contain forms that are not in the
  // cache (on pageload for example). In that case, the function creates and
  // adds a `FormStructure` to the cache (`context` should not be `std::nullopt`
  // in that case).
  void UpdateFormCache(base::span<const FormData> forms,
                       base::optional_ref<const AsyncContext> context,
                       FormStructure::RetrieveFromCacheReason reason,
                       bool preserve_signatures);

  std::unique_ptr<autofill_metrics::FormInteractionsUkmLogger>
  CreateFormInteractionsUkmLogger();

  // If `kAutofillSynchronousAfterParsing` is disabled:
  // Returns a callback that runs `callback` on the main thread after all
  // ongoing async parsing operations have finished.
  //
  // If `kAutofillSynchronousAfterParsing` is enabled (default behavior):
  // Just returns callback; enforces no asynchronicity.
  //
  // TODO(crbug.com/448144129): Remove once `kAutofillSynchronousAfterParsing`
  // can be cleaned up.
  template <typename... Args>
  base::OnceCallback<void(Args...)> AfterParsingFinishesDeprecated(
      base::OnceCallback<void(Args...)> callback);

  // Provides driver-level context to the shared code of the component.
  // `*driver_` owns this object.
  const raw_ref<AutofillDriver> driver_;

  // Observer needed to re-run heuristics when the language has been detected.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  // Our copy of the form data.
  std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures_;

  // Observers that listen to updates of this instance.
  base::ObserverList<Observer> observers_;

  // Set by SubscribeToMlModelChanges().
  base::CallbackListSubscription autofill_model_change_subscription_;
  base::CallbackListSubscription password_manager_model_change_subscription_;

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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_H_
