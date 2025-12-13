// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

}  // namespace password_manager
