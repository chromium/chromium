// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/password_form_user_action.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace autofill {
struct FormData;
}

namespace password_manager {

struct InteractionsStats;

// The pupose of this class is to record various types of metrics about the
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
                              ukm::SourceId source_id);

  // ManagerAction - What does the PasswordFormManager do with this form? Either
  // it fills it, or it doesn't. If it doesn't fill it, that's either
  // because it has no match or it is disabled via the AUTOCOMPLETE=off
  // attribute. Note that if we don't have an exact match, we still provide
  // candidates that the user may end up choosing.
  enum ManagerAction {
    kManagerActionNone = 0,
    kManagerActionAutofilled,
    kManagerActionBlacklisted_Obsolete,
    kManagerActionMax
  };

  // Same as above without the obsoleted 'Blacklisted' action.
  enum ManagerActionNew {
    kManagerActionNewNone = 0,
    kManagerActionNewAutofilled,
    kManagerActionNewMax
  };

  // Result - What happens to the form?
  enum SubmitResult {
    kSubmitResultNotSubmitted = 0,
    kSubmitResultFailed,
    kSubmitResultPassed,
    kSubmitResultMax
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

  // What the form is used for. kSubmittedFormTypeUnspecified is only set before
  // the SetSubmittedFormType() is called, and should never be actually
  // uploaded.
  enum SubmittedFormType {
    kSubmittedFormTypeLogin,
    kSubmittedFormTypeLoginNoUsername,
    kSubmittedFormTypeChangePasswordEnabled,
    kSubmittedFormTypeChangePasswordDisabled,
    kSubmittedFormTypeChangePasswordNoUsername,
    kSubmittedFormTypeSignup,
    kSubmittedFormTypeSignupNoUsername,
    kSubmittedFormTypeLoginAndSignup,
    kSubmittedFormTypeUnspecified,
    kSubmittedFormTypeMax
  };

  // The reason why a password bubble was shown on the screen.
  enum class BubbleTrigger {
    kUnknown = 0,
    // The password manager suggests the user to save a password and asks for
    // confirmation.
    kPasswordManagerSuggestionAutomatic,
    kPasswordManagerSuggestionManual,
    // The site asked the user to save a password via the credential management
    // API.
    kCredentialManagementAPIAutomatic,
    kCredentialManagementAPIManual,
    kMax,
  };

  // The reason why a password bubble was dismissed.
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

  // Result of comparing of the old and new form parsing for filling.
  enum class ParsingComparisonResult {
    kSame,
    kDifferent,
    // Old and new parsers use different identification mechanism for unnamed
    // fields, so the difference in parsing of anonymous fields is expected.
    kAnonymousFields,
    kMax
  };

  // Result of comparing of the old and new form parsing for saving. Multiple
  // values are meant to be combined and reported in a single number as a
  // bitmask.
  enum class ParsingOnSavingDifference {
    // Different fields were identified for username or password.
    kFields = 1 << 0,
    // Signon_realms are different.
    kSignonRealm = 1 << 1,
    // One password form manager wants to update, the other to save as new.
    kNewLoginStatus = 1 << 2,
    // One password form manager thinks the password is generated, the other
    // does not.
    kGenerated = 1 << 3,
  };

  // Indicator whether the user has seen a password generation popup and why.
  enum class PasswordGenerationPopupShown {
    kNotShown = 0,
    kShownAutomatically = 1,
    kShownManually = 2,
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
    kMaxFormDifferencesValue = 1 << 4,
  };

  // Used in UMA histogram, please do NOT reorder.
  // Metric: "PasswordManager.FirstWaitForUsernameReason"
  // This metric records why the browser instructs the renderer not to fill the
  // credentials on page load but to wait for the user to confirm the credential
  // to be filled. This decision is only recorded for the first time, the
  // browser informs the renderer about credentials for a given form.
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
    // The Touch To Fill feature is enabled.
    kTouchToFill = 5,
    // Show suggestion on account selection feature is enabled.
    kFoasFeature = 6,
    kMaxValue = kFoasFeature,
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
    // Domain is blacklisted and no other credentials exist.
    kNoSavedCredentialsAndBlacklisted = 7,
    // No credentials exist and the user has ignored the save bubble too often,
    // meaning that they won't be asked to save credentials anymore.
    kNoSavedCredentialsAndBlacklistedBySmartBubble = 8,
    kMaxValue = kNoSavedCredentialsAndBlacklistedBySmartBubble,
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

  // The maximum number of combinations of the ManagerAction, UserAction and
  // SubmitResult enums.
  // This is used when recording the actions taken by the form in UMA.
  static constexpr int kMaxNumActionsTaken =
      kManagerActionMax * static_cast<int>(UserAction::kMax) * kSubmitResultMax;

  // Same as above but for ManagerActionNew instead of ManagerAction.
  static constexpr int kMaxNumActionsTakenNew =
      kManagerActionNewMax * static_cast<int>(UserAction::kMax) *
      kSubmitResultMax;

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable();

  // Stores the user action associated with a generated password.
  void SetGeneratedPasswordStatus(GeneratedPasswordStatus status);

  // Stores the password manager action. During destruction the last
  // set value will be logged.
  void SetManagerAction(ManagerAction manager_action);

  // Calculates the user's action depending on the submitted form and existing
  // matches. Also inspects |manager_action_| to correctly detect if the
  // user chose a credential.
  void CalculateUserAction(
      const std::vector<const autofill::PasswordForm*>& best_matches,
      const autofill::PasswordForm& submitted_form);

  // Allow tests to explicitly set a value for |user_action_|.
  void SetUserActionForTesting(UserAction user_action);

  // Gets the current value of |user_action_|.
  UserAction GetUserAction() const;

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void LogSubmitPassed();
  void LogSubmitFailed();

  // This can be called multiple times in which case the last value is reported.
  void SetPasswordGenerationPopupShown(bool generation_popup_was_shown,
                                       bool is_manual_generation);

  // Call this once the submitted form type has been determined.
  void SetSubmittedFormType(SubmittedFormType form_type);

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

  // Converts the "ActionsTaken" fields (using ManagerActionNew) into an int so
  // they can be logged to UMA.
  // Public for testing.
  int GetActionsTakenNew() const;

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

  // Calculates FillingAssistance metric for |submitted_form|. The result is
  // stored in |filling_assistance_| and recorded in the destructor in case when
  // the successful submission is detected.
  void CalculateFillingAssistanceMetric(
      const autofill::FormData& submitted_form,
      const std::set<base::string16>& saved_usernames,
      const std::set<base::string16>& saved_passwords,
      bool is_blacklisted,
      const std::vector<InteractionsStats>& interactions_stats);

  // Calculates whether all field values in |submitted_form| came from
  // JavaScript. The result is stored in |js_only_input_|.
  void CalculateJsOnlyInput(const autofill::FormData& submitted_form);

  void set_user_typed_password_on_chrome_sign_in_page() {
    user_typed_password_on_chrome_sign_in_page_ = true;
  }

  void set_password_hash_saved_on_chrome_sing_in_page() {
    password_hash_saved_on_chrome_sing_in_page_ = true;
  }

  void set_possible_username_used(bool value) {
    possible_username_used_ = value;
  }

  void set_username_updated_in_bubble(bool value) {
    username_updated_in_bubble_ = value;
  }

 private:
  friend class base::RefCounted<PasswordFormMetricsRecorder>;

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

  // Converts the "ActionsTaken" fields into an int so they can be logged to
  // UMA.
  int GetActionsTaken() const;

  // True if the main frame's visible URL, at the time this PasswordFormManager
  // was created, is secure.
  const bool is_main_frame_secure_;

  // Whether the user can choose to generate a password for this form.
  bool generation_available_ = false;

  // Contains the generated password's status, which resulted from a user
  // action.
  base::Optional<GeneratedPasswordStatus> generated_password_status_;

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
  UserAction user_action_ = UserAction::kNone;
  SubmitResult submit_result_ = kSubmitResultNotSubmitted;

  // Form type of the form that the PasswordFormManager is managing. Set after
  // submission as the classification of the form can change depending on what
  // data the user has entered.
  SubmittedFormType submitted_form_type_ = kSubmittedFormTypeUnspecified;

  // The UKM SourceId of the document the form belongs to.
  ukm::SourceId source_id_;

  // Holds URL keyed metrics (UKMs) to be recorded on destruction.
  ukm::builders::PasswordForm ukm_entry_builder_;

  // Counter for DetailedUserActions observed during the lifetime of a
  // PasswordFormManager. Reported upon destruction.
  std::map<DetailedUserAction, int64_t> detailed_user_actions_counts_;

  // Bitmap of whether and why a manual fallback for saving was shown:
  // 1 = the fallback was shown.
  // 2 = the password was generated.
  // 4 = this was an update prompt.
  base::Optional<uint32_t> showed_manual_fallback_for_saving_;

  base::Optional<uint32_t> form_changes_bitmask_;

  bool recorded_first_filling_result_ = false;

  bool recorded_wait_for_username_reason_ = false;

  bool user_typed_password_on_chrome_sign_in_page_ = false;
  bool password_hash_saved_on_chrome_sing_in_page_ = false;

  base::Optional<FillingAssistance> filling_assistance_;

  bool possible_username_used_ = false;
  bool username_updated_in_bubble_ = false;

  base::Optional<JsOnlyInput> js_only_input_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormMetricsRecorder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_METRICS_RECORDER_H_
