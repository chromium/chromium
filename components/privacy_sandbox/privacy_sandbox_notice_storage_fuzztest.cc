// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace privacy_sandbox {

void CheckConvertsV1SchemaSuccessfully(int notice_action_taken,
                                       base::Time notice_taken_time,
                                       base::Time notice_last_shown) {
  V1MigrationData data_v1;
  data_v1.notice_action_taken =
      static_cast<NoticeActionTaken>(notice_action_taken);
  data_v1.notice_action_taken_time = notice_taken_time;
  data_v1.notice_last_shown = notice_last_shown;
  PrivacySandboxNoticeData data_v2 =
      PrivacySandboxNoticeStorage::ConvertV1SchemaToV2Schema(data_v1);
  EXPECT_EQ(data_v2.GetSchemaVersion(), 2);
  if (notice_action_taken) {
    auto notice_event = PrivacySandboxNoticeStorage::NoticeActionToNoticeEvent(
        static_cast<NoticeActionTaken>(notice_action_taken));
    if (notice_event.has_value()) {
      EXPECT_THAT(data_v2.GetNoticeEvents(),
                  testing::IsSupersetOf(
                      {std::make_pair(*notice_event, notice_taken_time)}));
    }
  }
  if (notice_last_shown != base::Time()) {
    EXPECT_THAT(data_v2.GetNoticeEvents(),
                testing::IsSupersetOf(
                    {std::make_pair(NoticeEvent::kShown, notice_last_shown)}));
  }
}

fuzztest::Domain<base::Time> AnyTime() {
  return fuzztest::Map(
      [](int64_t micros) {
        return base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(micros));
      },
      fuzztest::Arbitrary<int64_t>());
}

FUZZ_TEST(PrivacySandboxNoticeStorageFuzzTest,
          CheckConvertsV1SchemaSuccessfully)
    .WithDomains(/*notice_action_taken:*/ fuzztest::InRange(0, 9),
                 /*notice_taken_time:*/ AnyTime(),
                 /*notice_last_shown:*/ AnyTime());

}  // namespace privacy_sandbox
