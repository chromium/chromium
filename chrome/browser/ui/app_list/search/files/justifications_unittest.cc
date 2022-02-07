// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/justifications.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

class JustificationsTest : public testing::Test {
 protected:
  JustificationsTest() = default;
  ~JustificationsTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Returns a file path with its last_accessed set to |now - accessed_ago|, and
  // modified set to |now - modified_ago|.
  base::FilePath TouchFile(base::TimeDelta accessed_ago,
                           base::TimeDelta modified_ago) {
    auto now = base::Time::Now();
    base::FilePath path(temp_dir_.GetPath().Append("somefile"));
    base::WriteFile(path, "content");
    base::TouchFile(path, now - accessed_ago, now - modified_ago);
    return path;
  }

  base::FilePath TouchFile(base::TimeDelta time) {
    return TouchFile(time, time);
  }

  std::u16string Localized(int id) { return l10n_util::GetStringUTF16(id); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(JustificationsTest, OpenedOrEdited) {
  // Opened more recently than edited.
  EXPECT_EQ(
      GetJustificationString(TouchFile(base::Seconds(10), base::Seconds(30))),
      Localized(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Edited more recently than opened.
  EXPECT_EQ(
      GetJustificationString(TouchFile(base::Seconds(30), base::Seconds(10))),
      Localized(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));

  // When tied, should be edited.
  EXPECT_EQ(
      GetJustificationString(TouchFile(base::Seconds(30), base::Seconds(30))),
      Localized(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));
}

TEST_F(JustificationsTest, EditTimes) {
  // Just now.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Minutes(5))),
            Localized(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));

  // Today.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Hours(23))),
            Localized(IDS_APP_LIST_CONTINUE_EDITED_TODAY));

  // Yesterday.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Hours(47))),
            Localized(IDS_APP_LIST_CONTINUE_EDITED_YESTERDAY));

  // Past week.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Days(6))),
            Localized(IDS_APP_LIST_CONTINUE_EDITED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Days(30))),
            Localized(IDS_APP_LIST_CONTINUE_EDITED_PAST_MONTH));

  // No string, file too old.
  EXPECT_FALSE(GetJustificationString(TouchFile(base::Days(32))));
}

TEST_F(JustificationsTest, OpenTimes) {
  // Just now.
  EXPECT_EQ(
      GetJustificationString(TouchFile(base::Minutes(5), base::Days(100))),
      Localized(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Today.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Hours(23), base::Days(100))),
            Localized(IDS_APP_LIST_CONTINUE_OPENED_TODAY));

  // Yesterday.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Hours(47), base::Days(100))),
            Localized(IDS_APP_LIST_CONTINUE_OPENED_YESTERDAY));

  // Past week.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Days(6), base::Days(100))),
            Localized(IDS_APP_LIST_CONTINUE_OPENED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(GetJustificationString(TouchFile(base::Days(30), base::Days(100))),
            Localized(IDS_APP_LIST_CONTINUE_OPENED_PAST_MONTH));
}

}  // namespace app_list
