// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

// Unittests for small pieces of logic in `PasswordForm`.
TEST(PasswordFormTest, PasswordBackupNote) {
  PasswordForm form;

  form.SetPasswordBackupNote(u"backuppassword");

  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");
  EXPECT_EQ(form.GetPasswordBackupDateCreated(), form.notes[0].date_created);
}

TEST(PasswordFormTest, EmptyPasswordBackupNote) {
  PasswordForm form;

  form.SetPasswordBackupNote(u"");

  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_FALSE(form.GetPasswordBackup().has_value());
  EXPECT_FALSE(form.GetPasswordBackupDateCreated().has_value());
}

TEST(PasswordFormTest, DeletePasswordBackupNote) {
  PasswordForm form;
  form.SetPasswordBackupNote(u"backuppassword");
  EXPECT_EQ(form.notes[0].unique_display_name,
            PasswordNote::kPasswordChangeBackupNoteName);
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");

  form.DeletePasswordBackupNote();
  EXPECT_EQ(form.notes.size(), 0U);
  EXPECT_FALSE(form.GetPasswordBackup().has_value());
}

TEST(PasswordFormTest, RegularNote) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");

  EXPECT_EQ(form.notes[0].unique_display_name, u"");
  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");
}

TEST(PasswordFormTest, MixedNotes) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");
  form.SetPasswordBackupNote(u"backuppassword");

  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");
  EXPECT_EQ(form.GetPasswordBackup(), u"backuppassword");
  EXPECT_EQ(form.GetPasswordBackupDateCreated(), form.notes[1].date_created);
}

TEST(PasswordFormTest, UpdatesExistingNote) {
  PasswordForm form;

  form.SetNoteWithEmptyUniqueDisplayName(u"test note");
  ASSERT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");

  form.SetNoteWithEmptyUniqueDisplayName(u"updated note");
  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"updated note");
}

TEST(PasswordFormTest, SetPasswordBackupNoteUpdatesDateCreated) {
  PasswordForm form;
  form.SetPasswordBackupNote(u"first");
  std::optional<base::Time> first_date = form.GetPasswordBackupDateCreated();
  form.SetPasswordBackupNote(u"second");
  std::optional<base::Time> second_date = form.GetPasswordBackupDateCreated();

  ASSERT_TRUE(first_date);
  ASSERT_TRUE(second_date);
  EXPECT_GT(*second_date, *first_date);
}

}  // namespace

}  // namespace password_manager
