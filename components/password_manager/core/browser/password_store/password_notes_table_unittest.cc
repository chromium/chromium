// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_notes_table.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

PasswordForm CreatePasswordForm(std::u16string username = u"username") {
  PasswordForm form;
  form.signon_realm = "http://www.google.com";
  form.url = GURL(form.signon_realm);
  form.username_value = username;
  form.password_value = u"superstrongpassword";
  return form;
}

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

// TODO(crbug.com/40225853): Update the tests in this file to cover
// reading/writing of fields other than the note value.
class PasswordNotesTableTest : public testing::Test {
 protected:
  void SetUp() override {
    OSCryptMocker::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();
  }

  void TearDown() override {
    login_db_.reset();
    OSCryptMocker::TearDown();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");

    login_db_ = std::make_unique<LoginDatabase>(file, IsAccountStore(false));
    ASSERT_TRUE(login_db_->Init(base::NullCallback(), nullptr));
  }

  PasswordNotesTable* table() { return &login_db_->password_notes_table(); }

  LoginDatabase* login_db() { return login_db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LoginDatabase> login_db_;
};

TEST_F(PasswordNotesTableTest,
       WithSameParentIdThereCanBeOnlyOneAndItsReplaced) {
  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm()), SizeIs(1));

  const PasswordNote first_note(u"note 1", base::Time::Now()),
      second_note(u"note 2", base::Time::Now() + base::Hours(1)),
      third_note(u"note 3", base::Time::Now() + base::Hours(1));

  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), first_note));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), second_note));

  EXPECT_THAT(table()->GetPasswordNotes(FormPrimaryKey(1)),
              ElementsAre(second_note));
  EXPECT_THAT(table()->GetAllPasswordNotesForTest(), SizeIs(1));

  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), third_note));

  EXPECT_THAT(table()->GetPasswordNotes(FormPrimaryKey(1)),
              ElementsAre(third_note));
  EXPECT_THAT(table()->GetAllPasswordNotesForTest(), SizeIs(1));
}

TEST_F(PasswordNotesTableTest, ReloadingDatabasePersistsEntries) {
  const PasswordNote first_note(u"note 1", base::Time::Now()),
      second_note(u"note 2", base::Time::Now() + base::Hours(1));

  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user1")), SizeIs(1));
  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user2")), SizeIs(1));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), first_note));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(2), second_note));

  ReloadDatabase();

  EXPECT_THAT(table()->GetAllPasswordNotesForTest(),
              UnorderedElementsAre(
                  std::make_pair(FormPrimaryKey(1),
                                 std::vector<PasswordNote>({first_note})),
                  std::make_pair(FormPrimaryKey(2),
                                 std::vector<PasswordNote>({second_note}))));
}

TEST_F(PasswordNotesTableTest, GetPasswordNotes) {
  const PasswordNote first_note(u"note 1", base::Time::Now()),
      second_note(u"note 2", base::Time::Now() + base::Hours(1));

  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user1")), SizeIs(1));
  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user2")), SizeIs(1));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), first_note));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(2), second_note));

  EXPECT_THAT(table()->GetPasswordNotes(FormPrimaryKey(1)),
              ElementsAre(first_note));

  EXPECT_THAT(table()->GetPasswordNotes(FormPrimaryKey(2)),
              ElementsAre(second_note));
}

TEST_F(PasswordNotesTableTest, GetPasswordNotesWhenParentIdDoesntExist) {
  EXPECT_TRUE(table()->GetPasswordNotes(FormPrimaryKey(2)).empty());
}

TEST_F(PasswordNotesTableTest, RemovePasswordNotes) {
  const PasswordNote first_note(u"note 1", base::Time::Now()),
      second_note(u"note 2", base::Time::Now() + base::Hours(1)),
      third_note(u"note 3", base::Time::Now() + base::Hours(1));

  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user1")), SizeIs(1));
  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user2")), SizeIs(1));
  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user3")), SizeIs(1));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), first_note));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(2), second_note));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(3), third_note));

  EXPECT_TRUE(table()->RemovePasswordNotes(FormPrimaryKey(2)));

  EXPECT_THAT(
      table()->GetAllPasswordNotesForTest(),
      ElementsAre(std::make_pair(FormPrimaryKey(1),
                                 std::vector<PasswordNote>({first_note})),
                  std::make_pair(FormPrimaryKey(3),
                                 std::vector<PasswordNote>({third_note}))));
}

TEST_F(PasswordNotesTableTest, RemovePasswordNotesWithNonExistingKey) {
  const PasswordNote first_note(u"note 1", base::Time::Now());

  EXPECT_THAT(login_db()->AddLogin(CreatePasswordForm(u"user1")), SizeIs(1));
  EXPECT_TRUE(table()->InsertOrReplace(FormPrimaryKey(1), first_note));

  EXPECT_FALSE(table()->RemovePasswordNotes(FormPrimaryKey(1000)));
  EXPECT_THAT(table()->GetAllPasswordNotesForTest(),
              ElementsAre(std::make_pair(
                  FormPrimaryKey(1), std::vector<PasswordNote>({first_note}))));
}

}  // namespace
}  // namespace password_manager
