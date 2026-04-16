// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_proto_utils.h"

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/sync/protocol/password_specifics.pb.h"

using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

base::Time ConvertToBaseTime(uint64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because create_time_us has
      // always used the Windows epoch.
      base::Microseconds(time));
}

// Trims the notes field in the sync_pb::PasswordSpecificsData proto. If neither
// the high level notes field nor any of the individual notes contains populated
// fields, the high level field is cleared.
// LINT.IfChange(TrimPasswordSpecificsDataNotesForCaching)
void TrimPasswordSpecificsDataNotesForCaching(
    sync_pb::PasswordSpecificsData& trimmed_password_data) {
  // `notes` field should be cleared if all notes are empty.
  bool non_empty_note_exists = false;
  // Iterate over all notes and clear all supported fields.
  for (sync_pb::PasswordSpecificsData_Notes_Note& note :
       *trimmed_password_data.mutable_notes()->mutable_note()) {
    // Remember the `unique_display_name` such that if this note needs to be
    // cached, the `unique_display_name` is required to be able reconcile cached
    // notes during commit.
    std::string unique_display_name = note.unique_display_name();
    note.clear_unique_display_name();
    note.clear_value();
    note.clear_date_created_windows_epoch_micros();
    note.clear_hide_by_default();
    if (note.ByteSizeLong() != 0) {
      non_empty_note_exists = true;
      // Set the `unique_display_name` since it's required during the
      // reconciliation step in PasswordNotesToProto().
      note.set_unique_display_name(unique_display_name);
    }
  }
  if (non_empty_note_exists) {
    // Since some of the notes contain populated fields, no more trimming is
    // possible.
    return;
  } else {
    trimmed_password_data.mutable_notes()->clear_note();
  }
  // None of the individual notes contains populated fields. If the high level
  // Notes proto doesn't contain unknown fields either, we should clear the
  // notes field when trimming.
  if (trimmed_password_data.notes().unknown_fields().empty()) {
    trimmed_password_data.clear_notes();
  }
}
// LINT.ThenChange(//components/sync/protocol/password_specifics.proto:Notes)

}  // namespace

sync_pb::PasswordIssues PasswordIssuesMapToProto(
    const base::flat_map<InsecureType, InsecurityMetadata>&
        form_password_issues) {
  sync_pb::PasswordIssues password_issues;
  for (const auto& [insecure_type, insecure_metadata] : form_password_issues) {
    sync_pb::PasswordIssues::PasswordIssue issue;
    issue.set_date_first_detection_windows_epoch_micros(
        insecure_metadata.create_time.ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    issue.set_is_muted(insecure_metadata.is_muted.value());
    issue.set_trigger_notification_from_backend_on_detection(
        insecure_metadata.trigger_notification_from_backend.value());
    switch (insecure_type) {
      case InsecureType::kLeaked:
        DCHECK(!password_issues.has_leaked_password_issue());
        *password_issues.mutable_leaked_password_issue() = std::move(issue);
        break;
      case InsecureType::kPhished:
        DCHECK(!password_issues.has_phished_password_issue());
        *password_issues.mutable_phished_password_issue() = std::move(issue);
        break;
      case InsecureType::kWeak:
        DCHECK(!password_issues.has_weak_password_issue());
        *password_issues.mutable_weak_password_issue() = std::move(issue);
        break;
      case InsecureType::kReused:
        DCHECK(!password_issues.has_reused_password_issue());
        *password_issues.mutable_reused_password_issue() = std::move(issue);
        break;
    }
  }
  return password_issues;
}

InsecurityMetadata InsecurityMetadataFromProto(
    const sync_pb::PasswordIssues::PasswordIssue& issue) {
    return InsecurityMetadata(
        ConvertToBaseTime(issue.date_first_detection_windows_epoch_micros()),
        IsMuted(issue.is_muted()),
        TriggerBackendNotification(
            issue.trigger_notification_from_backend_on_detection()));
}

base::flat_map<InsecureType, InsecurityMetadata> PasswordIssuesMapFromProto(
    const sync_pb::PasswordSpecificsData& password_data) {
  base::flat_map<InsecureType, InsecurityMetadata> form_issues;
  const auto& specifics_issues = password_data.password_issues();
  if (specifics_issues.has_leaked_password_issue()) {
    const auto& issue = specifics_issues.leaked_password_issue();
    form_issues[InsecureType::kLeaked] = InsecurityMetadataFromProto(issue);
  }
  if (specifics_issues.has_reused_password_issue()) {
    const auto& issue = specifics_issues.reused_password_issue();
    form_issues[InsecureType::kReused] = InsecurityMetadataFromProto(issue);
  }
  if (specifics_issues.has_weak_password_issue()) {
    const auto& issue = specifics_issues.weak_password_issue();
    form_issues[InsecureType::kWeak] = InsecurityMetadataFromProto(issue);
  }
  if (specifics_issues.has_phished_password_issue()) {
    const auto& issue = specifics_issues.phished_password_issue();
    form_issues[InsecureType::kPhished] = InsecurityMetadataFromProto(issue);
  }
  return form_issues;
}

std::vector<PasswordNote> PasswordNotesFromProto(
    const sync_pb::PasswordSpecificsData_Notes& notes_proto) {
  std::vector<PasswordNote> notes;
  for (const sync_pb::PasswordSpecificsData_Notes_Note& note :
       notes_proto.note()) {
    notes.emplace_back(
        base::UTF8ToUTF16(note.unique_display_name()),
        base::UTF8ToUTF16(note.value()),
        ConvertToBaseTime(note.date_created_windows_epoch_micros()),
        note.hide_by_default());
  }
  return notes;
}

sync_pb::PasswordSpecificsData_Notes PasswordNotesToProto(
    const std::vector<PasswordNote>& notes,
    const sync_pb::PasswordSpecificsData_Notes& base_notes) {
  sync_pb::PasswordSpecificsData_Notes notes_proto = base_notes;
  for (const PasswordNote& note : notes) {
    sync_pb::PasswordSpecificsData_Notes_Note* note_proto = nullptr;
    // Try to find a corresponding cached note. Since `unique_display_name` is
    // unique per password, and immutable, it can be used to reconcile notes.
    // `unique_display_name` is cached in TrimPasswordSpecificsDataForCaching().
    for (sync_pb::PasswordSpecificsData_Notes_Note& cached_note :
         *notes_proto.mutable_note()) {
      if (cached_note.unique_display_name() ==
          base::UTF16ToUTF8(note.unique_display_name)) {
        note_proto = &cached_note;
        break;
      }
    }
    // If no corresponding cached note is found, add a new one.
    if (!note_proto) {
      note_proto = notes_proto.add_note();
      note_proto->set_unique_display_name(
          base::UTF16ToUTF8(note.unique_display_name));
    }

    note_proto->set_value(base::UTF16ToUTF8(note.value));
    note_proto->set_date_created_windows_epoch_micros(
        note.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
    note_proto->set_hide_by_default(note.hide_by_default);
  }
  return notes_proto;
}

sync_pb::PasswordSpecificsData TrimPasswordSpecificsDataForCaching(
    const sync_pb::PasswordSpecificsData& password_specifics_data) {
  // LINT.IfChange(TrimPasswordSpecificsDataForCaching)
  sync_pb::PasswordSpecificsData trimmed_password_data =
      sync_pb::PasswordSpecificsData(password_specifics_data);
  trimmed_password_data.clear_scheme();
  trimmed_password_data.clear_signon_realm();
  trimmed_password_data.clear_origin();
  trimmed_password_data.clear_action();
  trimmed_password_data.clear_username_element();
  trimmed_password_data.clear_username_value();
  trimmed_password_data.clear_password_element();
  trimmed_password_data.clear_password_value();
  trimmed_password_data.clear_date_created();
  trimmed_password_data.clear_blacklisted();
  trimmed_password_data.clear_type();
  trimmed_password_data.clear_times_used();
  trimmed_password_data.clear_display_name();
  trimmed_password_data.clear_avatar_url();
  trimmed_password_data.clear_federation_url();
  trimmed_password_data.clear_date_last_used();
  trimmed_password_data.clear_date_last_filled_windows_epoch_micros();
  trimmed_password_data.clear_password_issues();
  trimmed_password_data.clear_date_password_modified_windows_epoch_micros();
  trimmed_password_data.clear_sender_email();
  trimmed_password_data.clear_sender_name();
  trimmed_password_data.clear_date_received_windows_epoch_micros();
  trimmed_password_data.clear_sharing_notification_displayed();
  trimmed_password_data.clear_sender_profile_image_url();
  if (base::FeatureList::IsEnabled(
          features::kActorLoginSyncsPasswordPermissions)) {
    trimmed_password_data.clear_actor_login_approved();
  }
  // LINT.ThenChange(//components/sync/protocol/password_specifics.proto:PasswordSpecificsData)

  TrimPasswordSpecificsDataNotesForCaching(trimmed_password_data);

  return trimmed_password_data;
}

sync_pb::PasswordSpecifics SpecificsFromPassword(
    const PasswordForm& password_form,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  // WARNING: if you are adding support for new `PasswordSpecificsData` fields,
  // you need to update the following functions accordingly:
  // `TrimPasswordSpecificsDataForCaching`
  // `TrimAllSupportedFieldsFromRemoteSpecificsPreservesOnlyUnknownFields`
  DCHECK_EQ(0u, TrimPasswordSpecificsDataForCaching(
                    SpecificsDataFromPassword(password_form,
                                              /*base_password_data=*/{}))
                    .ByteSizeLong());

  sync_pb::PasswordSpecifics specifics;
  *specifics.mutable_client_only_encrypted_data() =
      SpecificsDataFromPassword(password_form, base_password_data);
  *specifics.mutable_unencrypted_metadata() =
      SpecificsMetadataFromPassword(password_form);
  return specifics;
}

sync_pb::PasswordSpecificsData SpecificsDataFromPassword(
    const PasswordForm& password_form,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  return SpecificsDataFromStoredCredential(FromPasswordForm(password_form),
                                           base_password_data);
}

sync_pb::PasswordSpecificsData SpecificsDataFromStoredCredential(
    const StoredCredential& credential) {
  return SpecificsDataFromStoredCredential(credential, {});
}

sync_pb::PasswordSpecificsData SpecificsDataFromStoredCredential(
    const StoredCredential& credential,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  sync_pb::PasswordSpecificsData password_data = base_password_data;
  password_data.set_scheme(static_cast<int>(credential.scheme));
  password_data.set_signon_realm(credential.signon_realm);
  password_data.set_origin(credential.url.is_valid() ? credential.url.spec()
                                                     : "");
  password_data.set_action(
      credential.action.is_valid() ? credential.action.spec() : "");
  password_data.set_username_element(
      base::UTF16ToUTF8(credential.username_element));
  password_data.set_password_element(
      base::UTF16ToUTF8(credential.password_element));
  password_data.set_username_value(
      base::UTF16ToUTF8(credential.username_value));
  password_data.set_password_value(
      base::UTF16ToUTF8(credential.password_value));
  password_data.set_date_last_used(
      credential.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_date_last_filled_windows_epoch_micros(
      credential.date_last_filled.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_date_password_modified_windows_epoch_micros(
      credential.date_password_modified.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  password_data.set_date_created(
      credential.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_blacklisted(credential.blocked_by_user);
  password_data.set_type(static_cast<int>(credential.type));
  password_data.set_times_used(credential.times_used_in_html_form);
  password_data.set_display_name(base::UTF16ToUTF8(credential.display_name));
  password_data.set_avatar_url(
      credential.icon_url.is_valid() ? credential.icon_url.spec() : "");
  password_data.set_federation_url(
      credential.federation_origin.IsValid()
          ? credential.federation_origin.Serialize()
          : std::string());
  *password_data.mutable_password_issues() =
      PasswordIssuesMapToProto(credential.password_issues);
  *password_data.mutable_notes() =
      PasswordNotesToProto(credential.notes, base_password_data.notes());
  password_data.set_sender_email(base::UTF16ToUTF8(credential.sender_email));
  password_data.set_sender_name(base::UTF16ToUTF8(credential.sender_name));
  password_data.set_date_received_windows_epoch_micros(
      credential.date_received.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_sharing_notification_displayed(
      credential.sharing_notification_displayed);
  password_data.set_sender_profile_image_url(
      credential.sender_profile_image_url.is_valid()
          ? credential.sender_profile_image_url.spec()
          : "");
  if (base::FeatureList::IsEnabled(
          features::kActorLoginSyncsPasswordPermissions)) {
    password_data.set_actor_login_approved(credential.actor_login_approved);
  }
  return password_data;
}

sync_pb::PasswordSpecifics SpecificsFromStoredCredential(
    const StoredCredential& credential) {
  return SpecificsFromStoredCredential(credential, {});
}

sync_pb::PasswordSpecifics SpecificsFromStoredCredential(
    const StoredCredential& credential,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  sync_pb::PasswordSpecifics specifics;
  *specifics.mutable_client_only_encrypted_data() =
      SpecificsDataFromStoredCredential(credential, base_password_data);

  sync_pb::PasswordSpecificsMetadata* password_metadata =
      specifics.mutable_unencrypted_metadata();
  password_metadata->set_url(credential.signon_realm);
  password_metadata->set_blacklisted(credential.blocked_by_user);
  password_metadata->set_date_last_used_windows_epoch_micros(
      credential.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  *password_metadata->mutable_password_issues() =
      PasswordIssuesMapToProto(credential.password_issues);
  password_metadata->set_type(static_cast<int>(credential.type));

  return specifics;
}

sync_pb::PasswordSpecificsMetadata SpecificsMetadataFromPassword(
    const PasswordForm& password_form) {
  sync_pb::PasswordSpecificsMetadata password_metadata;
  password_metadata.set_url(password_form.signon_realm);
  password_metadata.set_blacklisted(password_form.blocked_by_user);
  password_metadata.set_date_last_used_windows_epoch_micros(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  *password_metadata.mutable_password_issues() =
      PasswordIssuesMapToProto(password_form.password_issues);
  password_metadata.set_type(static_cast<int>(password_form.type));
  return password_metadata;
}

PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password_data) {
  PasswordForm password;
  password.scheme = static_cast<PasswordForm::Scheme>(password_data.scheme());
  password.signon_realm = password_data.signon_realm();
  password.url = GURL(password_data.origin());
  password.action = GURL(password_data.action());
  password.username_element =
      base::UTF8ToUTF16(password_data.username_element());
  password.password_element =
      base::UTF8ToUTF16(password_data.password_element());
  password.username_value = base::UTF8ToUTF16(password_data.username_value());
  password.password_value = base::UTF8ToUTF16(password_data.password_value());
  if (password_data.has_date_last_used()) {
    password.date_last_used = ConvertToBaseTime(password_data.date_last_used());
  } else if (password_data.preferred()) {
    // For legacy passwords that don't have the |date_last_used| field set, we
    // should it similar to the logic in login database migration.
    password.date_last_used =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
  }

  password.date_last_filled =
      ConvertToBaseTime(password_data.date_last_filled_windows_epoch_micros());
  password.date_password_modified = ConvertToBaseTime(
      password_data.has_date_password_modified_windows_epoch_micros()
          ? password_data.date_password_modified_windows_epoch_micros()
          : password_data.date_created());
  password.date_created = ConvertToBaseTime(password_data.date_created());
  password.blocked_by_user = password_data.blacklisted();
  password.type = static_cast<PasswordForm::Type>(password_data.type());
  password.times_used_in_html_form = password_data.times_used();
  password.display_name = base::UTF8ToUTF16(password_data.display_name());
  password.icon_url = GURL(password_data.avatar_url());
  password.federation_origin =
      url::SchemeHostPort(GURL(password_data.federation_url()));
  password.password_issues = PasswordIssuesMapFromProto(password_data);
  password.notes = PasswordNotesFromProto(password_data.notes());
  password.sender_email = base::UTF8ToUTF16(password_data.sender_email());
  password.sender_name = base::UTF8ToUTF16(password_data.sender_name());
  password.date_received =
      ConvertToBaseTime(password_data.date_received_windows_epoch_micros());
  password.sharing_notification_displayed =
      password_data.sharing_notification_displayed();
  password.sender_profile_image_url =
      GURL(password_data.sender_profile_image_url());
  if (base::FeatureList::IsEnabled(
          features::kActorLoginSyncsPasswordPermissions)) {
    password.actor_login_approved = password_data.actor_login_approved();
  }
  return password;
}

StoredCredential StoredCredentialFromSpecifics(
    const sync_pb::PasswordSpecificsData& password_data) {
  StoredCredential cred;
  cred.scheme = static_cast<PasswordForm::Scheme>(password_data.scheme());
  cred.signon_realm = password_data.signon_realm();
  cred.url = GURL(password_data.origin());
  cred.action = GURL(password_data.action());
  cred.username_element = base::UTF8ToUTF16(password_data.username_element());
  cred.password_element = base::UTF8ToUTF16(password_data.password_element());
  cred.username_value = base::UTF8ToUTF16(password_data.username_value());
  cred.password_value = base::UTF8ToUTF16(password_data.password_value());
  if (password_data.has_date_last_used()) {
    cred.date_last_used = ConvertToBaseTime(password_data.date_last_used());
  } else if (password_data.preferred()) {
    // For legacy passwords that don't have the |date_last_used| field set, we
    // should set it similar to the logic in login database migration.
    cred.date_last_used = base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
  }

  cred.date_last_filled =
      ConvertToBaseTime(password_data.date_last_filled_windows_epoch_micros());
  cred.date_password_modified = ConvertToBaseTime(
      password_data.has_date_password_modified_windows_epoch_micros()
          ? password_data.date_password_modified_windows_epoch_micros()
          : password_data.date_created());
  cred.date_created = ConvertToBaseTime(password_data.date_created());
  cred.blocked_by_user = password_data.blacklisted();
  cred.type = static_cast<PasswordForm::Type>(password_data.type());
  cred.times_used_in_html_form = password_data.times_used();
  cred.display_name = base::UTF8ToUTF16(password_data.display_name());
  cred.icon_url = GURL(password_data.avatar_url());
  cred.federation_origin =
      url::SchemeHostPort(GURL(password_data.federation_url()));
  cred.password_issues = PasswordIssuesMapFromProto(password_data);
  cred.notes = PasswordNotesFromProto(password_data.notes());
  cred.sender_email = base::UTF8ToUTF16(password_data.sender_email());
  cred.sender_name = base::UTF8ToUTF16(password_data.sender_name());
  cred.date_received =
      ConvertToBaseTime(password_data.date_received_windows_epoch_micros());
  cred.sharing_notification_displayed =
      password_data.sharing_notification_displayed();
  cred.sender_profile_image_url =
      GURL(password_data.sender_profile_image_url());
  if (base::FeatureList::IsEnabled(
          features::kActorLoginSyncsPasswordPermissions)) {
    cred.actor_login_approved = password_data.actor_login_approved();
  }
  return cred;
}

}  // namespace password_manager
