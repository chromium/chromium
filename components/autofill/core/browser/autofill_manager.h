// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_handler.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/field_filler.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"

namespace gfx {
class RectF;
}

namespace autofill {

class AutofillDataModel;
class AutofillDownloadManager;
class AutofillExternalDelegate;
class AutofillField;
class AutofillClient;
class AutofillManagerTestDelegate;
class AutofillProfile;
class AutofillType;
class CreditCard;
class FormStructureBrowserTest;
class LogManager;

struct FormData;
struct FormFieldData;

// We show the credit card signin promo only a certain number of times.
extern const int kCreditCardSigninPromoImpressionLimit;

// Enum for the value patterns metric. Don't renumerate existing value. They are
// used for metrics.
enum class ValuePatternsMetric {
  kNoPatternFound = 0,
  kUpiVpa = 1,  // UPI virtual payment address.
  kIban = 2,    // International Bank Account Number.
  kMaxValue = kIban,
};

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class AutofillManager : public AutofillHandler,
                        public AutofillDownloadManager::Observer,
                        public AutocompleteHistoryManager::SuggestionsHandler,
                        public CreditCardAccessManager::Accessor {
 public:
  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  const std::string& app_locale,
                  AutofillDownloadManagerState enable_download_manager);
  ~AutofillManager() override;

  // Sets an external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

  void ShowAutofillSettings(bool show_credit_card_settings);

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Returns the type of the popup being shown.
  virtual PopupType GetPopupType(const FormData& form,
                                 const FormFieldData& field);

  // Whether we should show the signin promo, based on the triggered |field|
  // inside the |form|.
  virtual bool ShouldShowCreditCardSigninPromo(const FormData& form,
                                               const FormFieldData& field);

  // Handlers for the "Show Cards From Account" row. This row should be shown to
  // users who have cards in their account and can use Sync Transport. Clicking
  // the row records the user's consent to see these cards on this device, and
  // refreshes the popup.
  virtual bool ShouldShowCardsFromAccountOption(const FormData& form,
                                                const FormFieldData& field);
  virtual void OnUserAcceptedCardsFromAccountOption();
  virtual void RefetchCardsAndUpdatePopup(int query_id,
                                          const FormData& form,
                                          const FormFieldData& field_data);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Returns the list of credit cards that have associated cloud token data.
  virtual void FetchVirtualCardCandidates();

  // Callback invoked when an actual card is selected. |selected_card_id| will
  // be used to identify the card. The selected card's cloud token data will be
  // fetched from the server.
  // TODO(crbug.com/1020740): Passes card server id for now. In the future when
  // one actual credit card can have multiple virtual cards, passes instrument
  // token instead. Design TBD.
  virtual void OnVirtualCardCandidateSelected(
      const std::string& selected_card_id);
#endif

  // Called from our external delegate so they cannot be private.
  virtual void FillOrPreviewForm(AutofillDriver::RendererFormDataAction action,
                                 int query_id,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 int unique_id);
  virtual void FillCreditCardForm(int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const CreditCard& credit_card,
                                  const base::string16& cvc);
  void DidShowSuggestions(bool has_autofill_suggestions,
                          const FormData& form,
                          const FormFieldData& field);

  // Called from autofill assistant.
  virtual void FillProfileForm(const autofill::AutofillProfile& profile,
                               const FormData& form,
                               const FormFieldData& field);

  // Returns true if the value/identifier is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body);

  // Remove the credit card or Autofill profile that matches |unique_id|
  // from the database. Returns true if deletion is allowed.
  bool RemoveAutofillProfileOrCreditCard(int unique_id);

  // Remove the specified Autocomplete entry.
  void RemoveAutocompleteEntry(const base::string16& name,
                               const base::string16& value);

  // Invoked when the user selected |value| in the Autocomplete drop-down.
  void OnAutocompleteEntrySelected(const base::string16& value);

  // Invoked when the user selects the "Hide Suggestions" item in the
  // Autocomplete drop-down.
  virtual void OnUserHideSuggestions(const FormData& form,
                                     const FormFieldData& field);

  // Returns true only if the previewed form should be cleared.
  bool ShouldClearPreviewedForm();

  AutofillClient* client() { return client_; }

  AutofillDownloadManager* download_manager() {
    return download_manager_.get();
  }

  CreditCardAccessManager* credit_card_access_manager() {
    return credit_card_access_manager_.get();
  }

  payments::FullCardRequest* GetOrCreateFullCardRequest();

  base::WeakPtr<payments::FullCardRequest::UIDelegate>
  GetAsFullCardRequestUIDelegate();

  const std::string& app_locale() const { return app_locale_; }

  // Only for testing.
  void SetTestDelegate(AutofillManagerTestDelegate* delegate);

  // Will send an upload based on the |form_structure| data and the local
  // Autofill profile data. |observed_submission| is specified if the upload
  // follows an observed submission event. Returns false if the upload couldn't
  // start.
  virtual bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission);

  // Update the pending form with |form|, possibly processing the current
  // pending form for upload.
  void UpdatePendingForm(const FormData& form);

  // Upload the current pending form.
  void ProcessPendingFormForUpload();

  // Invoked when the popup view can't be created. Main usage is to collect
  // metrics.
  void DidSuppressPopup(const FormData& form, const FormFieldData& field);

  // AutofillHandler:
  void OnFocusNoLongerOnForm() override;
  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;
  void OnDidPreviewAutofillFormData() override;
  void OnDidEndTextFieldEditing() override;
  void OnHidePopup() override;
  void SelectFieldOptionsDidChange(const FormData& form) override;
  void Reset() override;

  // AutocompleteHistoryManager::SuggestionsHandler:
  void OnSuggestionsReturned(
      int query_id,
      bool autoselect_first_suggestion,
      const std::vector<Suggestion>& suggestions) override;

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns true if the value of the AutofillProfileEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns true if the value of the AutofillCreditCardEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillCreditCardEnabled() const;

  // Shared code to determine if |form| should be uploaded to the Autofill
  // server. It verifies that uploading is allowed and |form| meets conditions
  // to be uploadable. Exposed for testing.
  bool ShouldUploadForm(const FormStructure& form);

  // Rich queries are enabled by feature flag iff this chrome instance is
  // neither on the STABLE nor BETA release channel.
  static bool IsRichQueryEnabled(version_info::Channel channel);

  // Returns the last form the autofill manager considered in this frame.
  virtual const FormData& last_query_form() const;

  // Exposed to ContentAutofillDriver to help with recording WebOTP metrics.
  bool has_parsed_forms() const { return has_parsed_forms_; }
  bool has_observed_phone_number_field() const {
    return has_observed_phone_number_field_;
  }
  bool has_observed_one_time_code_field() const {
    return has_observed_one_time_code_field_;
  }

#if defined(UNIT_TEST)
  // A public wrapper that calls |DeterminePossibleFieldTypesForUpload| for
  // testing purposes only.
  static void DeterminePossibleFieldTypesForUploadForTest(
      const std::vector<AutofillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards,
      const base::string16& last_unlocked_credit_card_cvc,
      const std::string& app_locale,
      FormStructure* submitted_form) {
    DeterminePossibleFieldTypesForUpload(profiles, credit_cards,
                                         last_unlocked_credit_card_cvc,
                                         app_locale, submitted_form);
  }

  // A public wrapper that calls |OnLoadedServerPredictions| for testing
  // purposes only.
  void OnLoadedServerPredictionsForTest(
      std::string response,
      const std::vector<FormSignature>& queried_form_signatures) {
    OnLoadedServerPredictions(response, queried_form_signatures);
  }

  // A public wrapper that calls |MakeFrontendID| for testing purposes only.
  int MakeFrontendIDForTest(const std::string& cc_backend_id,
                            const std::string& profile_backend_id) const {
    return MakeFrontendID(cc_backend_id, profile_backend_id);
  }

  // A public wrapper that calls |form_interactions_ukm_logger| for testing
  // purposes only.
  AutofillMetrics::FormInteractionsUkmLogger*
  form_interactions_ukm_logger_for_test() {
    return form_interactions_ukm_logger();
  }

  // A public wrapper that calls |ShouldTriggerRefill| for testing purposes
  // only.
  bool ShouldTriggerRefillForTest(const FormStructure& form_structure) {
    return ShouldTriggerRefill(form_structure);
  }

  // A public wrapper that calls |TriggerRefill| for testing purposes only.
  void TriggerRefillForTest(const FormData& form) { TriggerRefill(form); }
#endif

 protected:
  // Test code should prefer to use this constructor.
  AutofillManager(
      AutofillDriver* driver,
      AutofillClient* client,
      PersonalDataManager* personal_data,
      AutocompleteHistoryManager* autocomplete_history_manager,
      const std::string app_locale = "en-US",
      AutofillDownloadManagerState enable_download_manager =
          DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
      std::unique_ptr<CreditCardAccessManager> cc_access_manager = nullptr);

  // Uploads the form data to the Autofill server. |observed_submission|
  // indicates that upload is the result of a submission event.
  virtual void UploadFormData(const FormStructure& submitted_form,
                              bool observed_submission);

  // Logs quality metrics for the |submitted_form| and uploads the form data
  // to the crowdsourcing server, if appropriate. |observed_submission|
  // indicates whether the upload is a result of an observed submission event.
  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time,
      bool observed_submission);

  // Maps suggestion backend ID to and from an integer identifying it. Two of
  // these intermediate integers are packed by MakeFrontendID to make the IDs
  // that this class generates for the UI and for IPC.
  virtual int BackendIDToInt(const std::string& backend_id) const;
  virtual std::string IntToBackendID(int int_id) const;

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int MakeFrontendID(const std::string& cc_backend_id,
                     const std::string& profile_backend_id) const;
  void SplitFrontendID(int frontend_id,
                       std::string* cc_backend_id,
                       std::string* profile_backend_id) const;

  // AutofillHandler:
  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;
  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;
  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override {}
  void OnQueryFormFieldAutofillImpl(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& transformed_box,
                                    bool autoselect_first_suggestion) override;
  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;
  bool ShouldParseForms(const std::vector<FormData>& forms,
                        const base::TimeTicks timestamp) override;
  void OnFormsParsed(const std::vector<const FormData*>& forms,
                     const base::TimeTicks timestamp) override;

  AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger() {
    return form_interactions_ukm_logger_.get();
  }

  // Exposed for testing.
  void set_download_manager(AutofillDownloadManager* manager) {
    download_manager_.reset(manager);
  }

  // Exposed for testing.
  bool is_rich_query_enabled() const { return is_rich_query_enabled_; }

 private:
  // Keeps track of the filling context for a form, used to make refill attemps.
  struct FillingContext {
    // |optional_profile| or |optional_credit_card| must be non-null.
    // If |optional_credit_card| is non-null, |optional_cvc| may be non-null.
    FillingContext(const AutofillField& field,
                   const AutofillProfile* optional_profile,
                   const CreditCard* optional_credit_card,
                   const base::string16* optional_cvc);
    ~FillingContext();

    // Whether a refill attempt was made.
    bool attempted_refill = false;
    // The profile or credit card that was used for the initial fill.
    // The std::string associated with the credit card is the CVC, which may be
    // empty.
    const base::Optional<AutofillProfile> profile;
    const base::Optional<std::pair<CreditCard, base::string16>> credit_card;
    // The name of the field that was initially filled.
    const base::string16 filled_field_name;
    // The time at which the initial fill occurred.
    const base::TimeTicks original_fill_time;
    // The timer used to trigger a refill.
    base::OneShotTimer on_refill_timer;
    // The field type groups that were initially filled.
    std::set<FieldTypeGroup> type_groups_originally_filled;
  };

  // Indicates the reason why autofill suggestions are suppressed.
  enum class SuppressReason {
    kNotSuppressed,
    // Credit card suggestions are not shown because an ablation experiment is
    // enabled.
    kCreditCardsAblation,
    // Address suggestions are not shown because the field is annotated with
    // autocomplete=off and the directive is being observed by the browser.
    kAutocompleteOff,
    // Suggestions are not shown because this form is on a secure site, but
    // submits insecurely. This is only used when the user has started typing,
    // otherwise a warning is shown.
    kInsecureForm,
  };

  // The context for the list of suggestions available for a given field to be
  // returned by GetAvailableSuggestions().
  struct SuggestionsContext {
    FormStructure* form_structure = nullptr;
    AutofillField* focused_field = nullptr;
    bool is_autofill_available = false;
    bool is_context_secure = false;
    bool is_filling_credit_card = false;
    // Flag to indicate whether all suggestions come from Google Payments.
    bool should_display_gpay_logo = false;
    SuppressReason suppress_reason = SuppressReason::kNotSuppressed;
  };

  // AutofillDownloadManager::Observer:
  void OnLoadedServerPredictions(
      std::string response,
      const std::vector<FormSignature>& queried_form_signatures) override;

  // CreditCardAccessManager::Accessor
  void OnCreditCardFetched(
      bool did_succeed,
      const CreditCard* credit_card = nullptr,
      const base::string16& cvc = base::string16()) override;

  // Returns false if Autofill is disabled or if no Autofill data is available.
  bool RefreshDataModels();

  // Gets the card referred to by the guid |unique_id|. Returns |nullptr| if
  // card does not exist.
  CreditCard* GetCreditCard(int unique_id);

  // Gets the profile referred to by the guid |unique_id|. Returns |nullptr| if
  // profile does not exist.
  AutofillProfile* GetProfile(int unique_id);

  // Determines whether a fill on |form| initiated from |field| will wind up
  // filling a credit card number. This is useful to determine if we will need
  // to unmask a card.
  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field);

  // Fills or previews the credit card form.
  // Assumes the form and field are valid.
  void FillOrPreviewCreditCardForm(
      AutofillDriver::RendererFormDataAction action,
      int query_id,
      const FormData& form,
      const FormFieldData& field,
      const CreditCard* credit_card);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  void FillOrPreviewProfileForm(AutofillDriver::RendererFormDataAction action,
                                int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const AutofillProfile& profile);

  // Fills or previews |data_model| in the |form|.
  void FillOrPreviewDataModelForm(AutofillDriver::RendererFormDataAction action,
                                  int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const AutofillProfile* optional_profile,
                                  const CreditCard* optional_credit_card,
                                  const base::string16* optional_cvc,
                                  FormStructure* form_structure,
                                  AutofillField* autofill_field,
                                  bool is_refill = false);

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  std::unique_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  AutofillField* GetAutofillField(const FormData& form,
                                  const FormFieldData& field)
      WARN_UNUSED_RESULT;

  // Returns true if any form in the field corresponds to an address
  // |FieldTypeGroup|.
  bool FormHasAddressField(const FormData& form) WARN_UNUSED_RESULT;

  // Returns Suggestions corresponding to both the |autofill_field| type and
  // stored profiles whose values match the contents of |field|. |form| stores
  // data about the form with which the user is interacting, e.g. the number and
  // types of form fields.
  std::vector<Suggestion> GetProfileSuggestions(
      const FormStructure& form,
      const FormFieldData& field,
      const AutofillField& autofill_field) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  // |should_display_gpay_logo| will be set to true if there is no credit card
  // suggestions or all suggestions come from Payments server.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormFieldData& field,
      const AutofillType& type,
      bool* should_display_gpay_logo) const;

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Examines |form| and returns true if it is in a non-secure context or
  // its action attribute targets a HTTP url.
  bool IsFormNonSecure(const FormData& form) const;

  // Uses the existing personal data in |profiles| and |credit_cards| to
  // determine possible field types for the |submitted_form|.  This is
  // potentially expensive -- on the order of 50ms even for a small set of
  // |stored_data|. Hence, it should not run on the UI thread -- to avoid
  // locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
  static void DeterminePossibleFieldTypesForUpload(
      const std::vector<AutofillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards,
      const base::string16& last_unlocked_credit_card_cvc,
      const std::string& app_locale,
      FormStructure* submitted_form);

  // Uses context about previous and next fields to select the appropriate type
  // for fields with ambiguous upload types.
  static void DisambiguateUploadTypes(FormStructure* form);

  // Disambiguates address field upload types.
  static void DisambiguateAddressUploadTypes(FormStructure* form,
                                             size_t current_index);

  // Disambiguates phone field upload types.
  static void DisambiguatePhoneUploadTypes(FormStructure* form,
                                           size_t current_index);

  // Disambiguates name field upload types.
  static void DisambiguateNameUploadTypes(
      FormStructure* form,
      size_t current_index,
      const ServerFieldTypeSet& upload_types);

  void FillFieldWithValue(AutofillField* autofill_field,
                          const AutofillDataModel& data_model,
                          FormFieldData* field_data,
                          bool should_notify,
                          const base::string16& cvc,
                          uint32_t profile_form_bitmask,
                          std::string* failure_to_fill);

  // Whether there should be an attemps to refill the form. Returns true if all
  // the following are satisfied:
  //  There have been no refill on that page yet.
  //  A non empty form name was recorded in a previous fill
  //  That form name matched the currently parsed form name
  //  It's been less than kLimitBeforeRefillMs since the original fill.
  bool ShouldTriggerRefill(const FormStructure& form_structure);

  // Attempts to refill the form that was changed dynamically. Should only be
  // called if ShouldTriggerRefill returns true.
  void TriggerRefill(const FormData& form);

  // Replaces the contents of |suggestions| with available suggestions for
  // |field|. |context| will contain additional information about the
  // suggestions, such as if they correspond to credit card suggestions and
  // if the context is secure.
  void GetAvailableSuggestions(const FormData& form,
                               const FormFieldData& field,
                               std::vector<Suggestion>* suggestions,
                               SuggestionsContext* context);

  // Retrieves the page language from |client_|
  std::string GetPageLanguage() const override;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Whether to show the option to use virtual card in the autofill popup.
  bool ShouldShowVirtualCardOption(FormStructure* form_structure);
#endif

  // Returns an appropriate EventFormLogger for the given |field_type_group|.
  // May return nullptr.
  FormEventLoggerBase* GetEventFormLogger(
      FieldTypeGroup field_type_group) const;

  void SetDataList(const std::vector<base::string16>& values,
                   const std::vector<base::string16>& labels);
  AutofillClient* const client_;

  LogManager* log_manager_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  // Used to help fill data into fields.
  FieldFiller field_filler_;

  base::circular_deque<std::string> autofilled_form_signatures_;

  // Handles queries and uploads to Autofill servers. Will be NULL if
  // the download manager functionality is disabled.
  std::unique_ptr<AutofillDownloadManager> download_manager_;

  // Handles single-field autocomplete form data.
  // May be NULL.  NULL indicates OTR.
  base::WeakPtr<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Utility for logging URL keyed metrics.
  std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Utilities for logging form events.
  std::unique_ptr<AddressFormEventLogger> address_form_event_logger_;
  std::unique_ptr<CreditCardFormEventLogger> credit_card_form_event_logger_;

  // Have we logged whether Autofill is enabled for this page load?
  bool has_logged_autofill_enabled_ = false;
  // Have we logged an address suggestions count metric for this page?
  bool has_logged_address_suggestions_count_ = false;
  // Have we shown Autofill suggestions at least once?
  bool did_show_suggestions_ = false;
  // Has the user manually edited at least one form field among the autofillable
  // ones?
  bool user_did_type_ = false;
  // Has the user autofilled a form on this page?
  bool user_did_autofill_ = false;
  // Has the user edited a field that was previously autofilled?
  bool user_did_edit_autofilled_field_ = false;

  // Does |this| have any parsed forms?
  bool has_parsed_forms_ = false;
  // Is there a field with autocomplete="one-time-code" observed?
  bool has_observed_one_time_code_field_ = false;
  // Is there a field with phone number collection observed?
  bool has_observed_phone_number_field_ = false;

  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // A copy of the currently interacted form data.
  std::unique_ptr<FormData> pending_form_data_;

  // The credit card access manager, used to access local and server cards.
  std::unique_ptr<CreditCardAccessManager> credit_card_access_manager_;

  // The autofill offer manager, used to to retrieve offers for card
  // suggestions.
  AutofillOfferManager* offer_manager_;

  // Collected information about the autofill form where a credit card will be
  // filled.
  AutofillDriver::RendererFormDataAction credit_card_action_;
  int credit_card_query_id_ = -1;
  FormData credit_card_form_;
  FormFieldData credit_card_field_;
  CreditCard credit_card_;
  base::string16 last_unlocked_credit_card_cvc_;

  // Ablation experiment turns off autofill, but logging still has to be kept
  // for metrics analysis.
  bool enable_ablation_logging_ = false;

  // Suggestion backend ID to ID mapping. We keep two maps to convert back and
  // forth. These should be used only by BackendIDToInt and IntToBackendID.
  // Note that the integers are not frontend IDs.
  mutable std::map<std::string, int> backend_to_int_map_;
  mutable std::map<int, std::string> int_to_backend_map_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_ = nullptr;

  // Delegate used in test to get notifications on certain events.
  AutofillManagerTestDelegate* test_delegate_ = nullptr;

  // A map of form names to FillingContext instances used to make refill
  // attempts for dynamic forms.
  std::map<base::string16, std::unique_ptr<FillingContext>>
      filling_contexts_map_;

  // Tracks whether or not rich query encoding is enabled for this client.
  const bool is_rich_query_enabled_ = false;

  // Used to record metrics. This should be set at the beginning of the
  // interaction and re-used throughout the context of this manager.
  AutofillSyncSigninState sync_state_ = AutofillSyncSigninState::kNumSyncStates;

  base::WeakPtrFactory<AutofillManager> weak_ptr_factory_{this};

  friend class AutofillAssistantTest;
  friend class AutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class FormStructureBrowserTest;
  friend class GetMatchingTypesTest;
  friend class CreditCardAccessoryControllerTest;
  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
