// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/clock.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

class PrefService;

namespace autofill {
class FormData;
}

namespace password_manager {

struct InteractionsStats;

// The purpose of this class is to record various types of metrics about the
// behavior of the PasswordFormManager and its interaction with the user and
// the page. The recorder tracks events tied to the logical life of a password
// form, from parsing to having been saved. These events happen on different
// places in the code and the logical password form can be captured by multiple
// instances of real objects. To allow sharing the single recorder object among
// those, this class is refcounted. Reporting happens on destruction of the
// metrics recorder. Note that UKM metrics are reported for intervals of length
// metrics::GetUploadInterval(). Only metrics that are reported from the time
// of creating the PasswordFormMetricsRecorder until the end of current upload
// interval are recorded. Everything after the end of the current upload
// interval is discarded. For this reason, it is essential that references are
// not just kept until browser shutdown.
class PasswordFormMetricsRecorder
    : public base::RefCounted<PasswordFormMetricsRecorder> {
 public:
  // Records UKM metrics and reports them on destruction. The |source_id| is
  // the ID of the WebContents document that the forms belong to.
  PasswordFormMetricsRecorder(bool is_main_frame_secure,
                              ukm::SourceId source_id,
                              PrefService* pref_service);

  PasswordFormMetricsRecorder(const PasswordFormMetricsRecorder&) = delete;
  PasswordFormMetricsRecorder& operator=(const PasswordFormMetricsRecorder&) =
      delete;

  // ManagerAction - What does the PasswordFormManager do with this form? Either
  // it fills it, or it doesn't. If it doesn't fill it, that's either
  // because it has no match or it is disabled via the AUTOCOMPLETE=off
  // attribute. Note that if we don't have an exact match, we still provide
  // candidates that the user may end up choosing.
  enum ManagerAction {
    kManagerActionNone = 0,
    kManagerActionAutofilled,
    kManagerActionBlocklisted_Obsolete,
    kManagerActionMax
  };

  // Result - What happens to the form?
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Needs to stay in sync with PasswordFormSubmissionResult in enums.xml.
  enum class SubmitResult {
    kNotSubmitted = 0,
    kFailed = 1,
    kPassed = 2,
  };

  // Whether the password manager filled a credential on a form.
  enum ManagerAutofillEvent {
    // No credential existed that could be filled into a password form.
    kManagerFillEventNoCredential = 0,
    // A credential could have been autofilled into a password form but was not
    // due to a policy. E.g. incognito mode requires a user interaction before
    // filling can happen. PSL matches are not autofilled and also on password
    // change forms we do not autofill.
    kManagerFillEventBlockedOnInteraction,
    // A credential was autofilled into a form.
    kManagerFillEventAutofilled
  };

  // The reason why a password bubble was shown on the screen.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Needs to stay in sync with PasswordBubbleTrigger in enums.xml.
  enum class BubbleTrigger {
    kUnknown = 0,
    // The password manager suggests the user to save a password and asks for
    // confirmation.
    kPasswordManagerSuggestionAutomatic = 1,
    kPasswordManagerSuggestionManual = 2,
    // The site asked the user to save a password via the credential management
    // API.
    kCredentialManagementAPIAutomatic = 3,
    kCredentialManagementAPIManual = 4,
  };

  // The reason why a password bubble was dismissed.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Needs to stay in sync with PasswordBubbleDismissalReason in enums.xml.
  enum class BubbleDismissalReason {
    kUnknown = 0,
    kAccepted = 1,
    kDeclined = 2,
    kIgnored = 3
  };

  // This enum is a designed to be able to collect all kinds of potentially
  // interesting user interactions with sites and password manager UI in
  // relation to a given form. In contrast to UserAction, it is intended to be
  // extensible.
  enum class DetailedUserAction {
    // Interactions with password bubble.
    kEditedUsernameInBubble = 100,
    kSelectedDifferentPasswordInBubble = 101,
    kTriggeredManualFallbackForSaving = 102,
    kObsoleteTriggeredManualFallbackForUpdating = 103,  // unused

    // Interactions with form.
    kCorrectedUsernameInForm = 200,
  };

  // Indicator whether the user has seen a password generation popup and why.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Needs to stay in sync with PasswordGenerationPopupShown in enums.xml.
  enum class PasswordGenerationPopupShown {
    kNotShown = 0,
    kShownAutomatically = 1,
    kShownManually = 2,
    kMaxValue = kShownManually,
  };

  // Metric: PasswordGeneration.UserDecision
  enum class GeneratedPasswordStatus {
    // The generated password was accepted by the user.
    kPasswordAccepted = 0,
    // The generated password was edited by the user in the field in which
    // it was filled after being accepted.
    kPasswordEdited = 1,
    // The generated password was deleted by the user from the field
    // in which it was filled after being accepted.
    kPasswordDeleted = 2,
    kPasswordRejectedInDialogObsolete = 3,  // obsolete
    kMaxValue = kPasswordRejectedInDialogObsolete
  };

  // Represents form differences.
  // 1.This is a bit mask, so new values must be powers of 2.
  // 2.This is used for UMA, so no deletion, only adding at the end.
  enum FormDataDifferences {
    // Different number of fields.
    kFieldsNumber = 1 << 0,
    kRendererFieldIDs = 1 << 1,
    kAutocompleteAttributes = 1 << 2,
    kFormControlTypes = 1 << 3,
    kFormFieldNames = 1 << 4,
    kMaxFormDifferencesValue = 1 << 5,
  };

  // Used in UMA histogram, please do NOT reorder.
  // Metric: "PasswordManager.FirstWaitForUsernameReason"
  // This metric records why the browser instructs the renderer not to fill the
  // credentials on page load but to wait for the user to confirm the credential
  // to be filled. This decision is only recorded for the first time, the
  // browser informs the renderer about credentials for a given form.
  //
  // Needs to stay in sync with PasswordManagerFirstWaitForUsernameReason in
  // enums.xml.
  enum class WaitForUsernameReason {
    // Credentials may be filled on page load.
    kDontWait = 0,
    // User is browsing in incognito mode.
    kIncognitoMode = 1,
    // A credential exists for a PSL matched site but not for the current
    // security origin.
    kPublicSuffixMatch = 2,
    // Form is suspected to be a password change form. (Only recorded for old
    // form parser)
    kFormNotGoodForFilling = 3,
    // User is on a site with an insecure main frame origin.
    kInsecureOrigin = 4,
    // kTouchToFill = 5, Obsolete
    // Show suggestion on account selection feature is enabled.
    kFoasFeature = 6,
    // kReauthRequired = 7, Obsolete
    // Password is already filled
    kPasswordPrefilled = 8,
    // A credential exists for affiliated website.
    kAffiliatedWebsite = 9,
    // The form may accept WebAuthn credentials.
    kAcceptsWebAuthnCredentials = 10,
    // User need to reauthenticate using biometric.
    kBiometricAuthentication = 11,
    // Form is in an iframe with an origin that differs from the main frame
    // origin.
    kCrossOriginIframe = 12,
    // A credential with a different domain was grouped with the current domain
    // by the `AffiliationService`.
    kGroupedMatch = 13,
    kMaxValue = kGroupedMatch,
  };

  // Used in UMA histogram, please do NOT reorder.
  // Metric: "PasswordManager.MatchedFormType"
  // This metric records the type of the preferred password for filling. It is
  // recorded when the browser instructs the renderer to fill the credentials
  // on page load. This decision is only recorded for the first time, the
  // browser informs the renderer about credentials for a given form.
  //
  // Needs to stay in sync with PasswordManagerMatchedFormType in
  // enums.xml.
  enum class MatchedFormType {
    // The form is an exact match.
    kExactMatch = 0,
    // A credential exists for a PSL matched site but not for the current
    // security origin.
    kPublicSuffixMatch = 1,
    // A credential exists for an affiliated matched android app but not for the
    // current security origin. This is provided as a credential sharing
    // affiliation by AffiliationService.
    kAffiliatedApp = 2,
    // A credential exists for an affiliated matched site but not for the
    // current security origin. This is provided as a credential sharing
    // affiliation by AffiliationService.
    kAffiliatedWebsites = 3,
    // A credential exists for another entity, which is grouped with the current
    // domain by the AffiliationService through a grouping affiliation.
    kGrouped_Obsolete = 4,
    // A credential exists for an Android application, which is grouped with the
    // current domain by the AffiliationService through the grouping
    // affiliations.
    kGroupedApp = 5,
    // A credential exists for a website, which is grouped with the current
    // domain by the AffiliationService through the grouping affiliations.
    kGroupedWebsites = 6,
    kMaxValue = kGroupedWebsites,
  };

  // This metric records the user experience with the passwords filling. The
  // first 4 buckets are ranging from the best (automatic) to the worst (the
  // user has to type already saved password). Next 2 buckets showed the cases
  // when it was impossible to help because the unknown credentials were
  // submitted. The last bucket are strange cases, that the submitted form has
  // nor user input, nor autofilled data in password fields.
  enum class FillingAssistance {
    // Credential fields were filled automatically.
    kAutomatic = 0,
    // Credential fields were filled with involving manual filling (but none
    // required typing).
    kManual = 1,
    // Password was filled (automatically or manually), known username was
    // typed.
    kUsernameTypedPasswordFilled = 2,
    // Known password was typed.
    kKnownPasswordTyped = 3,
    // Unknown password was typed while some credentials were stored.
    kNewPasswordTypedWhileCredentialsExisted = 4,
    // No saved credentials.
    kNoSavedCredentials = 5,
    // Neither user input nor filling.
    kNoUserInputNoFillingInPasswordFields = 6,
    // Domain is blocklisted and no other credentials exist.
    kNoSavedCredentialsAndBlocklisted = 7,
    // No credentials exist and the user has ignored the save bubble too often,
    // meaning that they won't be asked to save credentials anymore.
    kNoSavedCredentialsAndBlocklistedBySmartBubble = 8,
    kMaxValue = kNoSavedCredentialsAndBlocklistedBySmartBubble,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Metric that records the user experience with filling the username in a
  // single username form.
  enum class SingleUsernameFillingAssistance {
    // Username was filled automatically.
    kAutomatic = 0,
    // Username was filled manually by using the fill UI without typing.
    kManual = 1,
    // Known username was typed - didn't fill while the username was available.
    // This may reflect an issue with filling saved credentials in the web
    // content.
    kKnownUsernameTyped = 2,
    // Unknown username was typed while some credentials were stored.
    kNewUsernameTypedWhileCredentialsExisted = 3,
    // No saved credentials.
    kNoSavedCredentials = 4,
    // Domain is blocklisted and no other credentials exist.
    kNoSavedCredentialsAndBlocklisted = 5,
    // No credentials exist and the user has ignored the save bubble too often,
    // meaning that they won't be asked to save credentials anymore.
    kNoSavedCredentialsAndBlocklistedBySmartBubble = 6,
    // Neither user input nor filling.
    kNoUserInputNoFillingOfUsername = 7,
    kMaxValue = kNoUserInputNoFillingOfUsername,
  };

  // Records which store(s) a filled password came from.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FillingSource {
    kNotFilled = 0,
    kFilledFromProfileStore = 1,
    kFilledFromAccountStore = 2,
    kFilledFromBothStores = 3,
    kMaxValue = kFilledFromBothStores,
  };

  // Records whether a password hash was saved or not on Chrome sign-in page.
  enum class ChromeSignInPageHashSaved {
    kPasswordTypedHashNotSaved = 0,
    kHashSaved = 1,
    kMaxValue = kHashSaved,
  };

  // Records user actions when Chrome suggests usernames on a page which are
  // considered to be username first flow.
  enum class SavingOnUsernameFirstFlow {
    kSaved = 0,
    kSavedWithEditedUsername = 1,
    kNotSaved = 2,
    kMaxValue = kNotSaved,
  };

  // Used in UMA histogram, please do NOT reorder.
  // Metric: "PasswordManager.JavaScriptOnlyValueInSubmittedForm"
  enum class JsOnlyInput {
    kOnlyJsInputNoFocus = 0,
    kOnlyJsInputWithFocus = 1,
    kAutofillOrUserInput = 2,
    kMaxValue = kAutofillOrUserInput,
  };

  // Used in UKM for difference on form parsing during filling and saving.
  // Do not reorder and keep in sync with PasswordFormParsingDifference
  // in enums.xml.
  enum class ParsingDifference {
    // The same username and password elements are identified.
    kNone = 0,
    // Different fields are picked as usernames, but password parsing is
    // consistent.
    kUsernameDiff = 1,
    // Different fields are picked as passwords, but username parsing is
    // consistent.
    kPasswordDiff = 2,
    // Both username and password parsing is inconsistent.
    kUsernameAndPasswordDiff = 3,
    kMaxValue = kUsernameAndPasswordDiff,
  };

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable();

  // Stores the user action associated with a generated password.
  void SetGeneratedPasswordStatus(GeneratedPasswordStatus status);

  // Stores the password manager action. During destruction the last
  // set value will be logged.
  void SetManagerAction(ManagerAction manager_action);

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void LogSubmitPassed();
  void LogSubmitFailed();

  // This can be called multiple times in which case the last value is reported.
  void SetPasswordGenerationPopupShown(bool generation_popup_was_shown,
                                       bool is_manual_generation);

  // Call this once the submitted form type has been determined.
  void SetSubmittedFormType(metrics_util::SubmittedFormType form_type);

  // Call this when a password is saved to indicate which path led to
  // submission.
  void SetSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event);

  // Records the event that a password bubble was shown.
  void RecordPasswordBubbleShown(
      metrics_util::CredentialSourceType credential_source_type,
      metrics_util::UIDisplayDisposition display_disposition);

  // Records the dismissal of a password bubble.
  void RecordUIDismissalReason(
      metrics_util::UIDismissalReason ui_dismissal_reason);

  // Records that the password manager managed or failed to fill a form.
  void RecordFillEvent(ManagerAutofillEvent event);

  // Records a DetailedUserAction UKM metric.
  void RecordDetailedUserAction(DetailedUserAction action);

  // Hash algorithm for RecordFormSignature. Public for testing.
  static int64_t HashFormSignature(autofill::FormSignature form_signature);

  // Records a low entropy hash of the form signature in order to be able to
  // distinguish two forms on the same site.
  void RecordFormSignature(autofill::FormSignature form_signature);

  // Records the readonly status encoded with parsing success after parsing for
  // filling. The |value| is constructed as follows: The least significant bit
  // says whether parsing succeeded (1) or not (0). The rest, shifted by one
  // bit to the right is the FormDataParser::ReadonlyPasswordFields
  // representation of the readonly status.
  void RecordReadonlyWhenFilling(uint64_t value);

  // Records the readonly status encoded with parsing success after parsing for
  // creating pending credentials. See RecordReadonlyWhenFilling for the meaning
  // of |value|.
  void RecordReadonlyWhenSaving(uint64_t value);

  // Records that Chrome noticed that it should show a manual fallback for
  // saving.
  void RecordShowManualFallbackForSaving(bool has_generated_password,
                                         bool is_update);

  void RecordFormChangeBitmask(uint32_t bitmask);

  void RecordFirstFillingResult(int32_t result);
  void RecordFirstWaitForUsernameReason(WaitForUsernameReason reason);
  void RecordMatchedFormType(const PasswordForm& form);
  void RecordPotentialPreferredMatch(std::optional<MatchedFormType> form_type);

  // Calculates FillingAssistance metrics for |submitted_form|.
  void CalculateFillingAssistanceMetric(
      const PasswordForm& submitted_form,
      const std::set<std::pair<std::u16string, PasswordForm::Store>>&
          saved_usernames,
      const std::set<std::pair<std::u16string, PasswordForm::Store>>&
          saved_passwords,
      bool is_blocklisted,
      const std::vector<InteractionsStats>& interactions_stats,
      features_util::PasswordAccountStorageUsageLevel
          account_storage_usage_level);

  // Calculates whether all field values in |submitted_form| came from
  // JavaScript. The result is stored in |js_only_input_|.
  void CalculateJsOnlyInput(const autofill::FormData& submitted_form);

  // Calculates the share of input text field characters in the submitted form
  // that are filled by Chrome. The result is stored in `automation_rate_`.
  void CalculateAutomationRate(const autofill::FormData& submitted_form);

  // Caches how the form was parsed for filling. Needed to measure the
  // difference in form parsing on filling and saving.
  void CacheParsingResultInFillingMode(const PasswordForm& form);

  // Calculates whether the password form was parsed in the same way
  // during parsing and saving.
  void CalculateParsingDifferenceOnSavingAndFilling(const PasswordForm& form);

  // Returns a string representation of the calculated `FillingAssistance` to be
  // used as part of hats survey answers information.
  // If the assistance holds a different variant type (i.e
  // `SingleUsernameFillingAssistance`), returns an empty string.
  std::string FillingAssinstanceToHatsInProductDataString();

  void set_possible_username_used(bool value) {
    possible_username_used_ = value;
  }

  void set_username_updated_in_bubble(bool value) {
    username_updated_in_bubble_ = value;
  }

  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

  void set_submitted_form_frame(
      metrics_util::SubmittedFormFrame submitted_form_frame) {
    submitted_form_frame_ = submitted_form_frame;
  }
#if BUILDFLAG(IS_ANDROID)
  void set_form_submission_reached(bool value) {
    form_submission_reached_ = value;
  }
#endif

 private:
  friend class base::RefCounted<PasswordFormMetricsRecorder>;

  // Calculates FillingAssistance metric for |submitted_form|. The result is
  // stored in |filling_assistance_| and recorded in the destructor in case when
  // the successful submission is detected.
  void CalculatePasswordFillingAssistanceMetric(
      const autofill::FormData& submitted_form,
      const std::set<std::pair<std::u16string, PasswordForm::Store>>&
          saved_usernames,
      const std::set<std::pair<std::u16string, PasswordForm::Store>>&
          saved_passwords,
      bool is_blocklisted,
      const std::vector<InteractionsStats>& interactions_stats,
      features_util::PasswordAccountStorageUsageLevel
          account_storage_usage_level);

  // Calculates the SingleUsernameFillingAssistance assistance metrics for the
  // |submitted_form| when it is a single username form.
  void CalculateSingleUsernameFillingAssistanceMetric(
      const autofill::FormData& submitted_form,
      const std::set<std::pair<std::u16string, PasswordForm::Store>>&
          saved_usernames,
      bool is_blocklisted,
      const std::vector<InteractionsStats>& interactions_stats);

  // Enum to track which password bubble is currently being displayed.
  enum class CurrentBubbleOfInterest {
    // This covers the cases that no password bubble is currently being
    // displayed or the one displayed is none of the interesting cases.
    kNone,
    // The user is currently seeing a password save bubble.
    kSaveBubble,
    // The user is currently seeing a password update bubble.
    kUpdateBubble,
  };

  // Destructor reports a couple of UMA metrics as well as calls
  // RecordUkmMetric.
  ~PasswordFormMetricsRecorder();

  // Not owned. Points to base::DefaultClock::GetInstance() by default, but can
  // be overridden for testing.
  raw_ptr<base::Clock> clock_;

  // True if the main frame's committed URL, at the time PasswordFormManager
  // was created, is secure.
  const bool is_main_frame_secure_;

  // Whether the user can choose to generate a password for this form.
  bool generation_available_ = false;

  // Contains the generated password's status, which resulted from a user
  // action.
  std::optional<GeneratedPasswordStatus> generated_password_status_;

  // Tracks which bubble is currently being displayed to the user.
  CurrentBubbleOfInterest current_bubble_ = CurrentBubbleOfInterest::kNone;

  // Whether the user was shown a prompt to update a password.
  bool update_prompt_shown_ = false;

  // Whether the user was shown a prompt to save a new credential.
  bool save_prompt_shown_ = false;

  // Whether the user was shown a password generation popup and why.
  // Only reportet when a popup was shown.
  PasswordGenerationPopupShown password_generation_popup_shown_ =
      PasswordGenerationPopupShown::kNotShown;

  // These three fields record the "ActionsTaken" by the browser and
  // the user with this form, and the result. They are combined and
  // recorded in UMA when the PasswordFormMetricsRecorder is destroyed.
  ManagerAction manager_action_ = kManagerActionNone;
  SubmitResult submit_result_ = SubmitResult::kNotSubmitted;

  // Presumed form type of the form that the PasswordFormManager is managing.
  // Set after submission, as the form type can change depending on the
  // user-entered data.
  std::optional<metrics_util::SubmittedFormType> submitted_form_type_;

  // The UKM SourceId of the document the form belongs to.
  ukm::SourceId source_id_;

  // Holds URL keyed metrics (UKMs) to be recorded on destruction.
  ukm::builders::PasswordForm ukm_entry_builder_;

  const raw_ptr<PrefService> pref_service_;

  // Counter for DetailedUserActions observed during the lifetime of a
  // PasswordFormManager. Reported upon destruction.
  std::map<DetailedUserAction, int64_t> detailed_user_actions_counts_;

  // Bitmap of whether and why a manual fallback for saving was shown:
  // 1 = the fallback was shown.
  // 2 = the password was generated.
  // 4 = this was an update prompt.
  std::optional<uint32_t> showed_manual_fallback_for_saving_;

  std::optional<uint32_t> form_changes_bitmask_;

  bool recorded_first_filling_result_ = false;

  bool recorded_wait_for_username_reason_ = false;

  bool recorded_preferred_matched_password_type = false;

  bool recorded_potential_preferred_matched_password_type = false;

  absl::variant<absl::monostate,
                FillingAssistance,
                SingleUsernameFillingAssistance>
      filling_assistance_;
  std::optional<FillingSource> filling_source_;
  std::optional<features_util::PasswordAccountStorageUsageLevel>
      account_storage_usage_level_;
  std::optional<metrics_util::SubmittedFormFrame> submitted_form_frame_;

  // Whether a single username candidate was populated in prompt.
  bool possible_username_used_ = false;

  bool username_updated_in_bubble_ = false;

  std::optional<JsOnlyInput> js_only_input_;

  bool is_mixed_content_form_ = false;

  // Renderer ids of key password form elements, saved on form filling.
  // Needed to measure the difference in form parsing on filling and saving.
  autofill::FieldRendererId username_rendered_id_;
  autofill::FieldRendererId password_rendered_id_;
  autofill::FieldRendererId new_password_rendered_id_;
  autofill::FieldRendererId confirmation_password_rendered_id_;

  std::optional<ParsingDifference> parsing_diff_on_filling_and_saving_;

  // Records the share of input text field characters in the submitted
  // form that are filled by Chrome. This value includes all fields in the
  // form (not only username and passwords).
  std::optional<float> automation_rate_;

#if BUILDFLAG(IS_ANDROID)
  // Set to true when the form submission step is reached. Used to record
  // form submission and avoid duplicate samples.
  bool form_submission_reached_ = false;
#endif
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
