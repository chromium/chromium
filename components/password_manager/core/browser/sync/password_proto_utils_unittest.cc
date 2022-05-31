// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_proto_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/protocol/list_passwords_result.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/password_with_local_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::Eq;

constexpr time_t kIssuesCreationTime = 1337;
constexpr char kTestOrigin[] = "https://www.origin.com/";
constexpr char kTestAction[] = "https://www.action.com/";
constexpr char kTestFormName[] = "login_form";
const std::u16string kTestFormName16(u"login_form");
constexpr char kTestUsernameElementName[] = "username_element";
const std::u16string kTestUsernameElementName16(u"username_element");
constexpr char kTestUsernameElementType[] = "text";
constexpr char kTestPasswordElementName[] = "password_element";
const std::u16string kTestPasswordElementName16(u"password_element");
constexpr char kTestPasswordElementType[] = "password";

sync_pb::PasswordSpecificsData_PasswordIssues CreateSpecificsDataIssues(
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordSpecificsData_PasswordIssues remote_issues;
  for (auto type : issue_types) {
    sync_pb::PasswordSpecificsData_PasswordIssues_PasswordIssue remote_issue;
    remote_issue.set_date_first_detection_microseconds(
        base::Time::FromTimeT(kIssuesCreationTime)
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    remote_issue.set_is_muted(false);
    switch (type) {
      case InsecureType::kLeaked:
        *remote_issues.mutable_leaked_password_issue() = remote_issue;
        break;
      case InsecureType::kPhished:
        *remote_issues.mutable_phished_password_issue() = remote_issue;
        break;
      case InsecureType::kWeak:
        *remote_issues.mutable_weak_password_issue() = remote_issue;
        break;
      case InsecureType::kReused:
        *remote_issues.mutable_reused_password_issue() = remote_issue;
        break;
    }
  }
  return remote_issues;
}

sync_pb::PasswordSpecificsData CreateSpecificsData(
    const std::string& origin,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordSpecificsData password_specifics;
  password_specifics.set_origin(origin);
  password_specifics.set_username_element(username_element);
  password_specifics.set_username_value(username_value);
  password_specifics.set_password_element(password_element);
  password_specifics.set_signon_realm(signon_realm);
  password_specifics.set_scheme(static_cast<int>(PasswordForm::Scheme::kHtml));
  password_specifics.set_action(GURL(origin).spec());
  password_specifics.set_password_value("D3f4ultP4$$w0rd");
  password_specifics.set_date_last_used(kIssuesCreationTime);
  password_specifics.set_date_created(kIssuesCreationTime);
  password_specifics.set_date_password_modified_windows_epoch_micros(
      kIssuesCreationTime);
  password_specifics.set_blacklisted(false);
  password_specifics.set_type(
      static_cast<int>(PasswordForm::Type::kFormSubmission));
  password_specifics.set_times_used(1);
  password_specifics.set_display_name("display_name");
  password_specifics.set_avatar_url(GURL(origin).spec());
  password_specifics.set_federation_url(std::string());
  *password_specifics.mutable_password_issues() =
      CreateSpecificsDataIssues(issue_types);
  // The current code always populates notes for outgoing protos even when
  // non-exists.
  password_specifics.mutable_notes();
  return password_specifics;
}

}  // namespace

TEST(PasswordProtoUtilsTest, ConvertIssueProtoToMapAndBack) {
  sync_pb::PasswordSpecificsData specifics_data =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          {InsecureType::kLeaked, InsecureType::kPhished,
                           InsecureType::kReused, InsecureType::kWeak});

  EXPECT_THAT(
      PasswordIssuesMapToProto(PasswordIssuesMapFromProto(specifics_data))
          .SerializeAsString(),
      Eq(specifics_data.password_issues().SerializeAsString()));
}

TEST(PasswordProtoUtilsTest, ConvertPasswordNoteToNotesProtoAndBack) {
  std::vector<PasswordNote> notes;
  notes.emplace_back(u"unique_display_name", u"value",
                     /*date_created*/ base::Time::Now(),
                     /*hide_by_default=*/true);
  notes.emplace_back(u"unique_display_name2", u"value2",
                     /*date_created*/ base::Time::Now() - base::Hours(1),
                     /*hide_by_default=*/false);
  sync_pb::PasswordSpecificsData_Notes base_notes_proto;
  EXPECT_EQ(notes, PasswordNotesFromProto(
                       PasswordNotesToProto(notes, base_notes_proto)));
}

TEST(PasswordProtoUtilsTest,
     CacheNoteUniqueDisplayNameWhenNoteContainsUnknownField) {
  const std::string kNoteUniqueDisplayName = "Note Unique Display Name";
  sync_pb::PasswordSpecificsData password_specifics_data;
  sync_pb::PasswordSpecificsData_Notes_Note* note =
      password_specifics_data.mutable_notes()->add_note();
  note->set_unique_display_name(kNoteUniqueDisplayName);
  *note->mutable_unknown_fields() = "unknown_fields";
  sync_pb::PasswordSpecificsData trimmed_specifics =
      TrimPasswordSpecificsDataForCaching(password_specifics_data);
  // The unique_display_name field should be cached since it's necessary for
  // reconciliation of notes with cached ones during commit.
  EXPECT_EQ(kNoteUniqueDisplayName,
            trimmed_specifics.notes().note(0).unique_display_name());
}

TEST(PasswordProtoUtilsTest, ReconcileCachedNotesUsingUnqiueDisplayName) {
  const std::string kNoteUniqueDisplayName1 = "Note Unique Display Name 1";
  const std::string kNoteValue1 = "Note Value 1";
  const std::string kNoteUnknownFields1 = "Note Unknown Fields 1";
  const std::string kNoteUniqueDisplayName2 = "Note Unique Display Name 2";
  const std::string kNoteValue2 = "Note Value 2";
  const std::string kNoteUnknownFields2 = "Note Unknown Fields 2";

  // Create a base note proto that contains two notes with unknown fields.
  sync_pb::PasswordSpecificsData_Notes base_notes;

  sync_pb::PasswordSpecificsData_Notes_Note* note_proto1 =
      base_notes.add_note();
  note_proto1->set_unique_display_name(kNoteUniqueDisplayName1);
  *note_proto1->mutable_unknown_fields() = kNoteUnknownFields1;

  sync_pb::PasswordSpecificsData_Notes_Note* note_proto2 =
      base_notes.add_note();
  note_proto2->set_unique_display_name(kNoteUniqueDisplayName2);
  *note_proto2->mutable_unknown_fields() = kNoteUnknownFields2;

  // Create the notes to be committed with the same unique display names in the
  // base specifics. Notes will be reconciled using the unique display name and
  // hence the order shouldn't matter.
  std::vector<PasswordNote> notes;
  notes.emplace_back(base::UTF8ToUTF16(kNoteUniqueDisplayName2),
                     base::UTF8ToUTF16(kNoteValue2),
                     /*date_created=*/base::Time::Now(),
                     /*hide_by_default=*/true);
  notes.emplace_back(base::UTF8ToUTF16(kNoteUniqueDisplayName1),
                     base::UTF8ToUTF16(kNoteValue1),
                     /*date_created=*/base::Time::Now(),
                     /*hide_by_default=*/true);

  // Reconciliation should preserve the order of the notes in the base specifics
  // and carry over the known fields.
  sync_pb::PasswordSpecificsData_Notes reconciled_notes =
      PasswordNotesToProto(notes, base_notes);
  EXPECT_EQ(kNoteUniqueDisplayName1,
            reconciled_notes.note(0).unique_display_name());
  EXPECT_EQ(kNoteValue1, reconciled_notes.note(0).value());
  EXPECT_EQ(kNoteUnknownFields1, reconciled_notes.note(0).unknown_fields());

  EXPECT_EQ(kNoteUniqueDisplayName2,
            reconciled_notes.note(1).unique_display_name());
  EXPECT_EQ(kNoteValue2, reconciled_notes.note(1).value());
  EXPECT_EQ(kNoteUnknownFields2, reconciled_notes.note(1).unknown_fields());
}

TEST(PasswordProtoUtilsTest, ConvertSpecificsToFormAndBack) {
  sync_pb::PasswordSpecifics specifics;
  *specifics.mutable_client_only_encrypted_data() =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          /*issue_types=*/{});

  EXPECT_THAT(SpecificsFromPassword(
                  PasswordFromSpecifics(specifics.client_only_encrypted_data()),
                  /*base_password_data=*/{})
                  .SerializeAsString(),
              Eq(specifics.SerializeAsString()));
}

TEST(PasswordProtoUtilsTest, SpecificsDataFromPasswordPreservesUnknownFields) {
  sync_pb::PasswordSpecificsData specifics =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          /*issue_types=*/{});

  PasswordForm form = PasswordFromSpecifics(specifics);

  *specifics.mutable_unknown_fields() = "unknown_fields";

  sync_pb::PasswordSpecificsData specifics_with_only_unknown_fields;
  *specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unknown_fields";

  EXPECT_EQ(SpecificsDataFromPassword(form, specifics_with_only_unknown_fields)
                .SerializeAsString(),
            specifics.SerializeAsString());
}

TEST(PasswordProtoUtilsTest, SpecificsFromPasswordPreservesUnknownFields) {
  sync_pb::PasswordSpecificsData specifics =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          /*issue_types=*/{});

  PasswordForm form = PasswordFromSpecifics(specifics);

  *specifics.mutable_unknown_fields() = "unknown_fields";

  sync_pb::PasswordSpecificsData specifics_with_only_unknown_fields;
  *specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unknown_fields";

  EXPECT_EQ(SpecificsFromPassword(form, specifics_with_only_unknown_fields)
                .client_only_encrypted_data()
                .SerializeAsString(),
            specifics.SerializeAsString());
}

TEST(PasswordProtoUtilsTest,
     ConvertPasswordWithLocalDataToFullPasswordFormAndBack) {
  sync_pb::PasswordWithLocalData password_data;
  *password_data.mutable_password_specifics_data() = CreateSpecificsData(
      kTestOrigin, kTestUsernameElementName, "username_value",
      kTestPasswordElementName, "signon_realm", {InsecureType::kLeaked});
  (*password_data.mutable_local_data())
      .set_previously_associated_sync_account_email("test@gmail.com");
  std::string opaque_metadata =
      "{\"form_data\":{\"action\":\"" + std::string(kTestAction) +
      "\",\"fields\":[{\"form_control_type\":\"" + kTestUsernameElementType +
      "\",\"name\":\"" + kTestUsernameElementName +
      "\"},{\"form_control_type\":\"" + kTestPasswordElementType +
      "\",\"name\":\"" + kTestPasswordElementName + "\"}],\"name\":\"" +
      kTestFormName + "\",\"url\":\"" + kTestOrigin +
      "\"},\"skip_zero_click\":false}";
  (*password_data.mutable_local_data()).set_opaque_metadata(opaque_metadata);

  PasswordForm form = PasswordFromProtoWithLocalData(password_data);
  EXPECT_THAT(form.url, Eq(GURL(kTestOrigin)));
  EXPECT_THAT(form.username_element, Eq(kTestUsernameElementName16));
  EXPECT_THAT(form.username_value, Eq(u"username_value"));
  EXPECT_THAT(form.password_element, Eq(kTestPasswordElementName16));
  EXPECT_THAT(form.signon_realm, Eq("signon_realm"));
  EXPECT_FALSE(form.skip_zero_click);
  EXPECT_EQ(form.form_data.url, GURL(kTestOrigin));
  EXPECT_EQ(form.form_data.action, GURL(kTestAction));
  EXPECT_EQ(form.form_data.name, kTestFormName16);
  ASSERT_EQ(form.form_data.fields.size(), 2u);
  EXPECT_EQ(form.form_data.fields[0].name, kTestUsernameElementName16);
  EXPECT_EQ(form.form_data.fields[0].form_control_type,
            kTestUsernameElementType);
  EXPECT_EQ(form.form_data.fields[1].name, kTestPasswordElementName16);
  EXPECT_EQ(form.form_data.fields[1].form_control_type,
            kTestPasswordElementType);

  sync_pb::PasswordWithLocalData password_data_converted_back =
      PasswordWithLocalDataFromPassword(form);
  EXPECT_EQ(password_data.SerializeAsString(),
            password_data_converted_back.SerializeAsString());
}

TEST(PasswordProtoUtilsTest, ConvertListResultToFormVector) {
  sync_pb::ListPasswordsResult list_result;
  sync_pb::PasswordWithLocalData password1;
  *password1.mutable_password_specifics_data() =
      CreateSpecificsData("http://1.origin.com/", "username_1", "username_1",
                          "password_1", "signon_1", {InsecureType::kLeaked});
  sync_pb::PasswordWithLocalData password2;
  *password2.mutable_password_specifics_data() =
      CreateSpecificsData("http://2.origin.com/", "username_2", "username_2",
                          "password_2", "signon_2", {InsecureType::kLeaked});
  *list_result.add_password_data() = password1;
  *list_result.add_password_data() = password2;

  std::vector<PasswordForm> forms = PasswordVectorFromListResult(list_result);

  EXPECT_THAT(forms, ElementsAre(PasswordFromProtoWithLocalData(password1),
                                 PasswordFromProtoWithLocalData(password2)));
}

}  // namespace password_manager
