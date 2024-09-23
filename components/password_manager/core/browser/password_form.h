// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_

#include <compare>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace password_manager {

// PasswordForm primary key which is used in the database.
using FormPrimaryKey = base::StrongAlias<class FormPrimaryKeyTag, int>;

// Represents a value, field renderer id, and the name of the element that
// contained the value. Used to determine whether another element must be
// selected as the right username or password field.
struct AlternativeElement {
  using Value =
      base::StrongAlias<class AlternativeElementValueTag, std::u16string>;
  using Name =
      base::StrongAlias<class AlternativeElementNameTag, std::u16string>;

  AlternativeElement(const Value& value,
                     autofill::FieldRendererId field_renderer_id,
                     const Name& name);
  explicit AlternativeElement(const Value& value);
  AlternativeElement(const AlternativeElement& rhs);
  AlternativeElement(AlternativeElement&& rhs);
  AlternativeElement& operator=(const AlternativeElement& rhs);
  AlternativeElement& operator=(AlternativeElement&& rhs);
  ~AlternativeElement();

  friend auto operator<=>(const AlternativeElement&,
                          const AlternativeElement&) = default;
  friend bool operator==(const AlternativeElement&,
                         const AlternativeElement&) = default;

  // The value of the field.
  std::u16string value;
  // The renderer id of the field. May be not set if the value is
  // not present in the submitted form.
  autofill::FieldRendererId field_renderer_id;
  // The name attribute of the field. May be empty if the value is
  // not present in the submitted form.
  std::u16string name;
};

#if defined(UNIT_TEST)
std::ostream& operator<<(std::ostream& os, const AlternativeElement& form);
#endif

// Vector of possible username or password values and corresponding field data.
using AlternativeElementVector = std::vector<AlternativeElement>;

using IsMuted = base::StrongAlias<class IsMutedTag, bool>;
using TriggerBackendNotification =
    base::StrongAlias<class TriggerBackendNotificationTag, bool>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InsecureType {
  // If the credentials was leaked by a data breach.
  kLeaked = 0,
  // If the credentials was entered on a phishing site.
  kPhished = 1,
  // If the password is weak.
  kWeak = 2,
  // If the password is reused for other accounts.
  kReused = 3,
  kMaxValue = kReused
};

// Metadata for insecure credentials
struct InsecurityMetadata {
  InsecurityMetadata();
  InsecurityMetadata(
      base::Time create_time,
      IsMuted is_muted,
      TriggerBackendNotification trigger_notification_from_backend);
  InsecurityMetadata(const InsecurityMetadata& rhs);
  ~InsecurityMetadata();

  friend bool operator==(const InsecurityMetadata& lhs,
                         const InsecurityMetadata& rhs) = default;

  // The date when the record was created.
  base::Time create_time;
  // Whether the problem was explicitly muted by the user.
  IsMuted is_muted{false};
  // Whether the backend should send a notification about the issue. True if
  // the user hasn't already been notified (e.g. via a leak check prompt).
  TriggerBackendNotification trigger_notification_from_backend{false};
};

// Represents a note attached to a particular credential.
struct PasswordNote {
  PasswordNote();
  PasswordNote(std::u16string value, base::Time date_created);
  PasswordNote(std::u16string unique_display_name,
               std::u16string value,
               base::Time date_created,
               bool hide_by_default);
  PasswordNote(const PasswordNote& rhs);
  PasswordNote(PasswordNote&& rhs);
  PasswordNote& operator=(const PasswordNote& rhs);
  PasswordNote& operator=(PasswordNote&& rhs);
  ~PasswordNote();

  friend bool operator==(const PasswordNote& lhs,
                         const PasswordNote& rhs) = default;

  // The name displayed in the UI labeling this note. Currently unused and added
  // for future compatibility.
  std::u16string unique_display_name;
  // The value of the note.
  std::u16string value;
  // The date when the note was created.
  base::Time date_created;
  // Whether the value of the note will be hidden by default in the UI similar
  // to password values. Currently unused and added for future compatibility.
  bool hide_by_default = false;
};

// The PasswordForm struct encapsulates information about a login form,
// which can be an HTML form or a dialog with username/password text fields.
//
// The Web Data database stores saved username/passwords and associated form
// metdata using a PasswordForm struct, typically one that was created from
// a parsed HTMLFormElement or LoginDialog, but the saved entries could have
// also been created by imported data from another browser.
//
// The PasswordManager implements a fuzzy-matching algorithm to compare saved
// PasswordForm entries against PasswordForms that were created from a parsed
// HTML or dialog form. As one might expect, the more data contained in one
// of the saved PasswordForms, the better the job the PasswordManager can do
// in matching it against the actual form it was saved on, and autofill
// accurately. But it is not always possible, especially when importing from
// other browsers with different data models, to copy over all the information
// about a particular "saved password entry" to our PasswordForm
// representation.
//
// The field descriptions in the struct specification below are intended to
// describe which fields are not strictly required when adding a saved password
// entry to the database and how they can affect the matching process.
struct PasswordForm {
  // Enum to differentiate between HTML form based authentication, and dialogs
  // using basic or digest schemes. Default is kHtml. Only PasswordForms of the
  // same Scheme will be matched/autofilled against each other.
  enum class Scheme {
    kHtml,
    kBasic,
    kDigest,
    kOther,
    kUsernameOnly,
    kMinValue = kHtml,
    kMaxValue = kUsernameOnly,
  };

  // Enum to differentiate between manually filled forms, forms with auto-
  // generated passwords, forms generated from the Credential Management
  // API and credentials manually added from setting.
  //
  // Always append new types at the end. This enum is converted to int and
  // stored in password store backends, so it is important to keep each
  // value assigned to the same integer.
  //
  // This might contain non-enum values: coming from clients that have a shorter
  // list of Type.
  enum class Type {
    kFormSubmission = 0,
    kGenerated = 1,
    kApi = 2,
    kManuallyAdded = 3,
    kImported = 4,
    kReceivedViaSharing = 5,
    kMinValue = kFormSubmission,
    kMaxValue = kReceivedViaSharing,
  };

  // Enum to keep track of what information has been sent to the server about
  // this form regarding password generation.
  enum class GenerationUploadStatus {
    kNoSignalSent,
    kPositiveSignalSent,
    kNegativeSignalSent,
    kMinValue = kNoSignalSent,
    kMaxValue = kNegativeSignalSent,
  };

  // Enum describing how PasswordForm was matched for a given FormDigest. This
  // enum is a bitmask because each PasswordForm can be matched by multiple
  // sources.
  enum class MatchType {
    // Default match type meaning signon_realm of a PasswordForm is identical to
    // a requested URL.
    kExact = 0,
    // signon_realm of a PasswordForm is affiliated with a given URL.
    // Affiliation information is provided by the affiliation service.
    kAffiliated = 1 << 1,
    // signon_realm of a PasswordForm has the same eTLD+1 as a given URL.
    kPSL = 1 << 2,
    // signon_realm of a PasswordForm is grouped with a given URL. Grouping
    // information is provided by the affiliation service.
    kGrouped = 1 << 3,
  };

  // The primary key of the password record in the logins database. This is only
  // set when the credentials has been read from the login database. Password
  // forms parsed from the web, or manually added in settings don't have this
  // field set. Also credentials read from sources other than logins database
  // (e.g. credential manager on Android) don't have this field set.
  std::optional<FormPrimaryKey> primary_key;

  Scheme scheme = Scheme::kHtml;

  // The "Realm" for the sign-on. This is scheme, host, port for SCHEME_HTML.
  // Dialog based forms also contain the HTTP realm. Android based forms will
  // contain a string of the form "android://<hash of cert>@<package name>"
  //
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // An URL consists of the scheme, host, port and path; the rest is stripped.
  // This is the primary data used by the PasswordManager to decide (in longest
  // matching prefix fashion) whether or not a given PasswordForm result from
  // the database is a good fit for a particular form on a page.
  GURL url;

  // The action target of the form; like |url|, consists of the scheme, host,
  // port and path; the rest is stripped. This is the primary data used by the
  // PasswordManager for form autofill; that is, the action of the saved
  // credentials must match the action of the form on the page to be autofilled.
  // If this is empty / not available, it will result in a "restricted" IE-like
  // autofill policy, where we wait for the user to type in their username
  // before autofilling the password. In these cases, after successful login the
  // action URL will automatically be assigned by the PasswordManager.
  //
  // When parsing an HTML form, this must always be set.
  GURL action;

  // The web realm affiliated with the Android application, if the form is an
  // Android credential. Otherwise, the string is empty. If there are several
  // realms affiliated with the application, an arbitrary realm is chosen. The
  // field is filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the string is empty.
  std::string affiliated_web_realm;

  // The display name (e.g. Play Store name) of the Android application if the
  // form is an Android credential. Otherwise, the string is empty. The field is
  // filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the string is empty.
  std::string app_display_name;

  // The icon URL (e.g. Play Store icon URL) of the Android application if the
  // form is an Android credential. Otherwise, the URL is empty. The field is
  // filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the URL is empty.
  GURL app_icon_url;

  // The name of the submit button used. Optional; only used in scoring
  // of PasswordForm results from the database to make matches as tight as
  // possible.
  std::u16string submit_element;

  // The name of the username input element.
  std::u16string username_element;

  // The renderer id of the username input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId username_element_renderer_id;

  // True if the server-side classification was successful.
  bool server_side_classification_successful = false;

  // True if the server-side classification believes that the field may be
  // pre-filled with a placeholder in the value attribute. It is set during
  // form parsing and not persisted.
  bool username_may_use_prefilled_placeholder = false;

  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  std::u16string username_value;

  // This member is populated in cases where we there are multiple possible
  // username values. Used to populate a dropdown for possible usernames.
  // Optional.
  AlternativeElementVector all_alternative_usernames;

  // This member is populated in cases where we there are multiple possible
  // password values. Used in pending password state, to populate a dropdown
  // for possible passwords. Contains all possible passwords. Optional.
  AlternativeElementVector all_alternative_passwords;

  // True if |all_alternative_passwords| have autofilled value or its part.
  bool form_has_autofilled_value = false;

  // The name of the input element corresponding to the current password.
  // Optional (improves scoring).
  //
  // When parsing an HTML form, this will always be set, unless it is a sign-up
  // form or a change password form that does not ask for the current password.
  // In these two cases the |new_password_element| will always be set.
  std::u16string password_element;

  // The renderer id of the password input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId password_element_renderer_id;

  // The current password. Must be non-empty for PasswordForm instances that are
  // meant to be persisted to the password store.
  //
  // When parsing an HTML form, this is typically empty.
  std::u16string password_value;

  // The current keychain identifier where the password is stored password. Only
  // non-empty on iOS for PasswordForm instances retrieved from the password
  // store or coming in a PasswordStoreChange that is not of type REMOVE.
  std::string keychain_identifier;

  // If the form was a sign-up or a change password form, the name of the input
  // element corresponding to the new password. Optional, and not persisted.
  std::u16string new_password_element;

  // The renderer id of the new password input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId new_password_element_renderer_id;

  // The confirmation password element. Optional, only set on form parsing, and
  // not persisted.
  std::u16string confirmation_password_element;

  // The renderer id of the confirmation password input element. It is set
  // during the new form parsing and not persisted.
  autofill::FieldRendererId confirmation_password_element_renderer_id;

  // The new password. Optional, and not persisted.
  std::u16string new_password_value;

  // When the login was last used by the user to login to the site. Defaults to
  // |date_created|, except for passwords that were migrated from the now
  // deprecated |preferred| flag. Their default is set when migrating the login
  // database to have the "date_last_used" column.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_last_used;

  // When the password value was last changed. The date can be unset on the old
  // credentials because the passwords wasn't modified yet. The code must keep
  // it in mind and fallback to 'date_last_used' or 'date_created'.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_password_modified;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blocked_by_user = false;

  // The form type.
  // This might contain non-enum values: coming from clients that have a shorter
  // list of Type.
  Type type = Type::kFormSubmission;

  // The number of times that this username/password has been used to
  // authenticate the user in an HTML form.
  //
  // When parsing an HTML form, this is not used.
  int times_used_in_html_form = 0;

  // Autofill representation of this form. Used to communicate with the
  // Autofill servers if necessary. Currently this is only used to help
  // determine forms where we can trigger password generation.
  //
  // When parsing an HTML form, this is normally set.
  autofill::FormData form_data;

  // What information has been sent to the Autofill server about this form.
  GenerationUploadStatus generation_upload_status =
      GenerationUploadStatus::kNoSignalSent;

  // These following fields are set by a website using the Credential Manager
  // API. They will be empty and remain unused for sites which do not use that
  // API.
  //
  // User friendly name to show in the UI.
  std::u16string display_name;

  // The URL of this credential's icon, such as the user's avatar, to display
  // in the UI.
  GURL icon_url;

  // The origin of identity provider used for federated login.
  url::SchemeHostPort federation_origin;

  // If true, Chrome will not return this credential to a site in response to
  // 'navigator.credentials.request()' without user interaction.
  // Once user selects this credential the flag is reseted.
  bool skip_zero_click = false;

  // If true, this form was parsed using Autofill predictions.
  bool was_parsed_using_autofill_predictions = false;

  // Only available when PasswordForm was requested though
  // PasswordStoreInterface::GetLogins(), empty otherwise.
  std::optional<MatchType> match_type;

  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is not persisted and filled
  // out only for submitted forms.
  autofill::mojom::SubmissionIndicatorEvent submission_event =
      autofill::mojom::SubmissionIndicatorEvent::NONE;

  // True iff heuristics declined this form for normal saving, updating, or
  // filling (e.g. only credit card fields were found). But this form can be
  // saved or filled only with the fallback.
  bool only_for_fallback = false;

  // True iff the form may be filled with webauthn credentials from an active
  // webauthn request.
  bool accepts_webauthn_credentials = false;

  // Serialized to prefs, so don't change numeric values!
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Store {
    // Default value.
    kNotSet = 0,
    // Credential came from the profile (i.e. local) storage.
    kProfileStore = 1 << 0,
    // Credential came from the Gaia-account-scoped storage.
    kAccountStore = 1 << 1,
    kMaxValue = kAccountStore
  };

  // Please use IsUsingAccountStore and IsUsingProfileStore to check in which
  // store the form is present.
  // TODO(crbug.com/40178769): Rename to in_stores to reflect possibility of
  // password presence in both stores.
  Store in_store = Store::kNotSet;

  // Vector of hashes of the gaia id for users who prefer not to move this
  // password form to their account. This list is used to suppress the move
  // prompt for those users.
  std::vector<signin::GaiaIdHash> moving_blocked_for_list;

  // A mapping from the credential insecurity type (e.g. leaked, phished),
  // to its metadata (e.g. time it was discovered, whether alerts are muted).
  base::flat_map<InsecureType, InsecurityMetadata> password_issues;

  // Attached notes to the credential.
  std::vector<PasswordNote> notes;

  // Email address of the last sync account this password was associated with.
  // This field is non empty only if the password is NOT currently associated
  // with a syncing account AND it was associated with one in the past.
  std::string previously_associated_sync_account_email;

  // Shared Password Metadata:
  // For credentials that have been shared by another user, this field captures
  // the sender email. It's empty for credentials that weren't received via
  // password sharing feature.
  std::u16string sender_email;
  // Similar to `sender_email` but for the sender name.
  std::u16string sender_name;
  // The URL of the profile image of the password sender to be displayed in the
  // UI.
  GURL sender_profile_image_url;
  // The time when the password was received via sharing feature from another
  // user.
  base::Time date_received;
  // Whether the user has been already notified that they received this password
  // from another user via the password sharing feature.
  bool sharing_notification_displayed = false;

  // Returns true if this form is considered to be a login form, i.e. it has
  // a username field, a password field and no new password field. It's based
  // on heuristics and may be inaccurate.
  bool IsLikelyLoginForm() const;

  // Returns true if we consider this form to be a signup form, i.e. it has
  // a username field, a new password field and no current password field. It's
  // based on heuristics and may be inaccurate.
  bool IsLikelySignupForm() const;

  // Returns true if we consider this form to be a change password form, i.e.
  // it has a current password field and a new password field. It's based on
  // heuristics and may be inaccurate.
  bool IsLikelyChangePasswordForm() const;

  // Returns true if we consider this form to be a reset password form, i.e.
  // it has a new password field and no current password field or username.
  // It's based on heuristics and may be inaccurate.
  bool IsLikelyResetPasswordForm() const;

  // Returns the `PasswordFormClassification::Type` classification of this form.
  // Note that just as `IsLikelyLoginForm()`, `IsLikelySignupForm()`, etc. this
  // prediction is based on heuristics and may be inaccurate.
  autofill::PasswordFormClassification::Type GetPasswordFormType() const;

  // Returns true if current password element is set.
  bool HasUsernameElement() const;

  // Returns true if current password element is set.
  bool HasPasswordElement() const;

  // Returns true if current password element is set.
  bool HasNewPasswordElement() const;

  // True iff |federation_origin| isn't empty.
  bool IsFederatedCredential() const;

  // True if username element is set and password and new password elements are
  // not set.
  bool IsSingleUsername() const;

  // Returns whether this form is stored in the account-scoped store.
  bool IsUsingAccountStore() const;

  // Returns whether this form is stored in the profile-scoped store.
  bool IsUsingProfileStore() const;

  // Returns true when |password_value| or |new_password_value| are non-empty.
  bool HasNonEmptyPasswordValue() const;

  // Returns the value of the note with an empty `unique_display_name`, returns
  // an empty string if none exists.
  std::u16string GetNoteWithEmptyUniqueDisplayName() const;

  // Updates the note with an empty `unique_display_name`.
  void SetNoteWithEmptyUniqueDisplayName(const std::u16string& new_note_value);

  PasswordForm();
  PasswordForm(const PasswordForm& other);
  PasswordForm(PasswordForm&& other);
  ~PasswordForm();

  PasswordForm& operator=(const PasswordForm& form);
  PasswordForm& operator=(PasswordForm&& form);

#if defined(UNIT_TEST)
  // An exact equality comparison of all the fields is only useful for tests.
  // Production code should be using `ArePasswordFormUniqueKeysEqual` instead.
  friend bool operator==(const PasswordForm&, const PasswordForm&) = default;
  friend bool operator!=(const PasswordForm& lhs,
                         const PasswordForm& rhs) = default;
#endif
};

// True if the unique keys for the forms are the same. The unique key is
// (url, username_element, username_value, password_element, signon_realm).
inline auto PasswordFormUniqueKey(const PasswordForm& f) {
  return std::tie(f.signon_realm, f.url, f.username_element, f.username_value,
                  f.password_element);
}
bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right);

// For testing.
#if defined(UNIT_TEST)
std::ostream& operator<<(std::ostream& os, PasswordForm::Scheme scheme);
std::ostream& operator<<(std::ostream& os, const PasswordForm& form);
std::ostream& operator<<(std::ostream& os, PasswordForm* form);
#endif  // defined(UNIT_TEST)

constexpr PasswordForm::Store operator&(PasswordForm::Store lhs,
                                        PasswordForm::Store rhs) {
  return static_cast<PasswordForm::Store>(static_cast<int>(lhs) &
                                          static_cast<int>(rhs));
}

constexpr PasswordForm::Store operator|(PasswordForm::Store lhs,
                                        PasswordForm::Store rhs) {
  return static_cast<PasswordForm::Store>(static_cast<int>(lhs) |
                                          static_cast<int>(rhs));
}

constexpr PasswordForm::MatchType operator&(PasswordForm::MatchType lhs,
                                            PasswordForm::MatchType rhs) {
  return static_cast<PasswordForm::MatchType>(static_cast<int>(lhs) &
                                              static_cast<int>(rhs));
}

constexpr PasswordForm::MatchType operator|(PasswordForm::MatchType lhs,
                                            PasswordForm::MatchType rhs) {
  return static_cast<PasswordForm::MatchType>(static_cast<int>(lhs) |
                                              static_cast<int>(rhs));
}

constexpr void operator|=(std::optional<PasswordForm::MatchType>& lhs,
                          PasswordForm::MatchType rhs) {
  lhs = lhs.has_value() ? (lhs.value() | rhs) : rhs;
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
