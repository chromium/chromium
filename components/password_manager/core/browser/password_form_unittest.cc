// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_string.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

class PasswordFormTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Unittests for small pieces of logic in `PasswordForm`.
TEST_F(PasswordFormTest, PasswordBackupNote) {
  PasswordForm form;

  form.SetPasswordBackupNote(u"backuppassword");

  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");
  EXPECT_EQ(form.GetPasswordBackupDateCreated(), base::Time::Now());
}

TEST_F(PasswordFormTest, EmptyPasswordBackupNote) {
  PasswordForm form;

  form.SetPasswordBackupNote(u"");

  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_FALSE(form.GetPasswordBackup().has_value());
  EXPECT_FALSE(form.GetPasswordBackupDateCreated().has_value());
}

TEST_F(PasswordFormTest, DeletePasswordBackupNote) {
  PasswordForm form;
  form.SetPasswordBackupNote(u"backuppassword");
  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");

  form.DeletePasswordBackupNote();
  EXPECT_EQ(form.notes.size(), 0U);
  EXPECT_FALSE(form.GetPasswordBackup().has_value());
}

TEST_F(PasswordFormTest, RegularNote) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");

  EXPECT_EQ(form.notes[0].unique_display_name, u"");
  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");
}

TEST_F(PasswordFormTest, MixedNotes) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");
  form.SetPasswordBackupNote(u"backuppassword");

  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");
  EXPECT_EQ(form.GetPasswordBackupDateCreated(), base::Time::Now());
}

TEST_F(PasswordFormTest, UpdatesExistingNote) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");
  ASSERT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");

  form.SetNoteWithEmptyUniqueDisplayName(u"updated note");
  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"updated note");
}

TEST_F(PasswordFormTest, SetPasswordBackupNoteUpdatesDateCreated) {
  PasswordForm form;

  form.SetPasswordBackupNote(u"first");
  base::Time first_date = form.GetPasswordBackupDateCreated().value();
  task_environment_.FastForwardBy(base::Seconds(1));
  form.SetPasswordBackupNote(u"second");

  EXPECT_EQ(form.GetPasswordBackupDateCreated(), base::Time::Now());
  EXPECT_EQ(first_date, base::Time::Now() - base::Seconds(1));
}

constexpr char16_t kPassword[] = u"hunter2";
constexpr char16_t kNewPassword[] = u"correct horse battery staple";
constexpr char16_t kOtherPassword[] = u"a-completely-different-password";

PasswordForm CreateFormWithPasswords() {
  PasswordForm form;
  form.url = GURL("https://example.com/login");
  form.signon_realm = "https://example.com/";
  form.username_element = u"username";
  form.username_value = u"user@example.com";
  form.password_element = u"password";
  form.password_value = PasswordString(std::u16string(kPassword));
  form.new_password_element = u"new_password";
  form.new_password_value = PasswordString(std::u16string(kNewPassword));
  return form;
}

// `PasswordForm::password_value` and `PasswordForm::new_password_value` are
// `PasswordString objects, so the copy, move and equality semantics of the
// whole struct are driven by that type. `PasswordString` picks its backing
// store (plain `std::u16string` vs. `crypto::ProcessBoundU16String`) based on
// `kUseProcessBoundPasswordString`, so every case below is run against both
// backings to guarantee the two are observationally equivalent.
class PasswordFormPasswordStringTest : public testing::TestWithParam<bool> {
 public:
  PasswordFormPasswordStringTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kUseProcessBoundPasswordString, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PasswordFormPasswordStringTest, CopyConstructionPreservesPasswords) {
  PasswordForm form = CreateFormWithPasswords();

  PasswordForm copy(form);

  // The copy decrypts to the same plaintext as the source, ...
  EXPECT_EQ(copy.password_value, kPassword);
  EXPECT_EQ(copy.new_password_value, kNewPassword);
  // ... and the source is left fully intact.
  EXPECT_EQ(form.password_value, kPassword);
  EXPECT_EQ(form.new_password_value, kNewPassword);
  EXPECT_EQ(form, copy);
}

TEST_P(PasswordFormPasswordStringTest, CopyAssignmentPreservesPasswords) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm copy;

  copy = form;

  EXPECT_EQ(copy.password_value, kPassword);
  EXPECT_EQ(copy.new_password_value, kNewPassword);
  EXPECT_EQ(form.password_value, kPassword);
  EXPECT_EQ(form.new_password_value, kNewPassword);
  EXPECT_EQ(form, copy);
}

// A copy must own its passwords outright: re-encrypting into an independent
// buffer rather than aliasing the source's buffer. Mutating either side must
// therefore leave the other untouched.
TEST_P(PasswordFormPasswordStringTest, CopiesOwnIndependentPasswords) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm copy(form);

  copy.password_value = PasswordString(std::u16string(kOtherPassword));
  copy.new_password_value.clear();

  EXPECT_EQ(copy.password_value, kOtherPassword);
  EXPECT_TRUE(copy.new_password_value.empty());
  EXPECT_EQ(form.password_value, kPassword);
  EXPECT_EQ(form.new_password_value, kNewPassword);
  EXPECT_NE(form, copy);

  // Mutating the source likewise leaves the copy alone.
  form.password_value.clear();
  EXPECT_TRUE(form.password_value.empty());
  EXPECT_EQ(copy.password_value, kOtherPassword);
}

TEST_P(PasswordFormPasswordStringTest,
       MoveConstructionTransfersAndClearsSource) {
  PasswordForm form = CreateFormWithPasswords();

  PasswordForm moved(std::move(form));

  EXPECT_EQ(moved.password_value, kPassword);
  EXPECT_EQ(moved.new_password_value, kNewPassword);

  // The moved-from form must not keep the passwords alive
  // These are intentional use-after-move to validate the state of the
  // password value after the move is complete, ensuring the password is not
  // still present
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(form.password_value.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, form.password_value.size());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(form.new_password_value.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, form.new_password_value.size());
}

TEST_P(PasswordFormPasswordStringTest, MoveAssignmentTransfersAndClearsSource) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm moved;
  moved.password_value = PasswordString(std::u16string(kOtherPassword));

  moved = std::move(form);

  EXPECT_EQ(moved.password_value, kPassword);
  EXPECT_EQ(moved.new_password_value, kNewPassword);

  // The moved-from form must not keep the passwords alive
  // These are intentional use-after-move to validate the state of the
  // password value after the move is complete, ensuring the password is not
  // still present
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(form.password_value.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, form.password_value.size());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(form.new_password_value.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, form.new_password_value.size());
}

// Moving out of a form and then reassigning it must yield a form that is
// indistinguishable from a freshly built one
TEST_P(PasswordFormPasswordStringTest, MovedFromFormIsReusable) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm moved(std::move(form));

  // Intentional use-after-move to validate the behavior of the password_value
  // field being reassigned after being moved out of.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  form.password_value = PasswordString(std::u16string(kOtherPassword));

  EXPECT_EQ(form.password_value, kOtherPassword);
  EXPECT_EQ(std::u16string(kOtherPassword).size(), form.password_value.size());
  // The form that was moved into is unaffected by the reuse.
  EXPECT_EQ(moved.password_value, kPassword);
}

TEST_P(PasswordFormPasswordStringTest,
       EqualityHoldsForIndependentlyBuiltForms) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm other = CreateFormWithPasswords();

  // Equality is defined on the plaintext, so two forms built separately compare
  // equal even though their backing buffers are distinct objects.
  EXPECT_EQ(form, other);
}

TEST_P(PasswordFormPasswordStringTest,
       EqualityHoldsForDefaultConstructedForms) {
  EXPECT_EQ(PasswordForm(), PasswordForm());
}

TEST_P(PasswordFormPasswordStringTest, EqualityDistinguishesPasswordValue) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm other = CreateFormWithPasswords();

  other.password_value = PasswordString(std::u16string(kOtherPassword));

  EXPECT_NE(form, other);
}

// Passwords of equal length take the full comparison path in
// `PasswordString::operator==` rather than the size-based short circuit.
TEST_P(PasswordFormPasswordStringTest,
       EqualityDistinguishesSameLengthPasswords) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm other = CreateFormWithPasswords();

  other.password_value = PasswordString(u"hunter3");
  ASSERT_EQ(form.password_value.size(), other.password_value.size());

  EXPECT_NE(form, other);
}

TEST_P(PasswordFormPasswordStringTest, EqualityDistinguishesNewPasswordValue) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm other = CreateFormWithPasswords();

  other.new_password_value = PasswordString(std::u16string(kOtherPassword));

  EXPECT_NE(form, other);
}

TEST_P(PasswordFormPasswordStringTest, EqualityDistinguishesEmptyPassword) {
  PasswordForm form = CreateFormWithPasswords();
  PasswordForm other = CreateFormWithPasswords();

  other.password_value.clear();

  EXPECT_NE(form, other);
  EXPECT_TRUE(other.password_value.empty());
}

INSTANTIATE_TEST_SUITE_P(ProcessBoundBacking,
                         PasswordFormPasswordStringTest,
                         testing::Bool());

}  // namespace

}  // namespace password_manager
