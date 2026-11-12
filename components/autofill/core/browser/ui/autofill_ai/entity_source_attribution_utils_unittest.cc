// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_ai/entity_source_attribution_utils.h"

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using GmailSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::GmailSourceMetadata;
using PhotosSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::PhotosSourceMetadata;
using Source = EntityInstance::PersonalContextRecordTypePayload::Source;

constexpr base::Time kTestTime =
    base::Time::FromSecondsSinceUnixEpoch(1700000000);

TEST(EntitySourceAttributionUtilsTest, GetSourceDisplayTitle_GmailWithTitle) {
  Source source{
      .url = GURL("https://mail.google.com"),
      .metadata = GmailSourceMetadata{.title = "Flight confirmation",
                                      .timestamp = kTestTime},
  };
  EXPECT_EQ(GetSourceDisplayTitle(source), u"Flight confirmation");
}

TEST(EntitySourceAttributionUtilsTest,
     GetSourceDisplayTitle_GmailWithoutTitle) {
  Source source{
      .url = GURL("https://mail.google.com"),
      .metadata = GmailSourceMetadata{.timestamp = kTestTime},
  };
  EXPECT_EQ(GetSourceDisplayTitle(source),
            base::TimeFormatShortDate(kTestTime));
}

TEST(EntitySourceAttributionUtilsTest, GetSourceDisplayTitle_GmailEmpty) {
  Source source{
      .url = GURL("https://mail.google.com"),
      .metadata = GmailSourceMetadata{},
  };
  EXPECT_EQ(GetSourceDisplayTitle(source), u"");
}

TEST(EntitySourceAttributionUtilsTest,
     GetSourceDisplayTitle_PhotosWithTimestamp) {
  Source source{
      .url = GURL("https://photos.google.com"),
      .metadata = PhotosSourceMetadata{.timestamp = kTestTime},
  };
  EXPECT_EQ(GetSourceDisplayTitle(source),
            base::TimeFormatShortDate(kTestTime));
}

TEST(EntitySourceAttributionUtilsTest, GetSourceDisplayTitle_PhotosEmpty) {
  Source source{
      .url = GURL("https://photos.google.com"),
      .metadata = PhotosSourceMetadata{},
  };
  EXPECT_EQ(GetSourceDisplayTitle(source), u"");
}

}  // namespace
}  // namespace autofill
