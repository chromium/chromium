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
  EXPECT_EQ(form.GetPasswordBackupNote(), u"backuppassword");
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
  EXPECT_EQ(form.GetPasswordBackupNote(), u"backuppassword");
}

TEST(PasswordFormTest, UpdatesExistingNote) {
  PasswordForm form;
  form.SetNoteWithEmptyUniqueDisplayName(u"test note");
  ASSERT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"test note");

  form.SetNoteWithEmptyUniqueDisplayName(u"updated note");
  EXPECT_EQ(form.GetNoteWithEmptyUniqueDisplayName(), u"updated note");
}

}  // namespace

}  // namespace password_manager
