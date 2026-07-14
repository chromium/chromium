// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/theme_utils.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "components/themes/ntp_custom_background_service_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace themes {
namespace {

const char kValidBackgroundUrl[] = "https://www.foo.com/";
const char kValidActionUrl[] = "https://action.com/";
const char kAttributionLine1[] = "line1";
const char kAttributionLine2[] = "line2";
const char kCollectionId[] = "collection";
const char kResumeToken[] = "resume_token";

TEST(ThemeUtilsTest, GetBackgroundDictFromProto) {
  sync_pb::NtpCustomBackground proto;
  proto.set_url(kValidBackgroundUrl);
  proto.set_attribution_line_1(kAttributionLine1);
  proto.set_attribution_line_2(kAttributionLine2);
  proto.set_attribution_action_url(kValidActionUrl);
  proto.set_collection_id(kCollectionId);
  proto.set_resume_token(kResumeToken);
  proto.set_refresh_timestamp_unix_epoch_seconds(123456789);
  proto.set_main_color(16711680);

  base::DictValue dict = GetBackgroundDictFromProto(proto);
  EXPECT_FALSE(dict.empty());

  const std::string* url = dict.FindString(kNtpCustomBackgroundURL);
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, kValidBackgroundUrl);

  const std::string* attr1 =
      dict.FindString(kNtpCustomBackgroundAttributionLine1);
  ASSERT_TRUE(attr1);
  EXPECT_EQ(*attr1, kAttributionLine1);

  const std::string* attr2 =
      dict.FindString(kNtpCustomBackgroundAttributionLine2);
  ASSERT_TRUE(attr2);
  EXPECT_EQ(*attr2, kAttributionLine2);

  const std::string* action_url =
      dict.FindString(kNtpCustomBackgroundAttributionActionURL);
  ASSERT_TRUE(action_url);
  EXPECT_EQ(*action_url, kValidActionUrl);

  const std::string* collection =
      dict.FindString(kNtpCustomBackgroundCollectionId);
  ASSERT_TRUE(collection);
  EXPECT_EQ(*collection, kCollectionId);

  const std::string* token = dict.FindString(kNtpCustomBackgroundResumeToken);
  ASSERT_TRUE(token);
  EXPECT_EQ(*token, kResumeToken);

  std::optional<int> timestamp =
      dict.FindInt(kNtpCustomBackgroundRefreshTimestamp);
  ASSERT_TRUE(timestamp.has_value());
  EXPECT_EQ(*timestamp, 123456789);

  std::optional<int> color = dict.FindInt(kNtpCustomBackgroundMainColor);
  ASSERT_TRUE(color.has_value());
  EXPECT_EQ(*color, 16711680);
}

TEST(ThemeUtilsTest, GetProtoFromBackgroundDict) {
  base::DictValue dict;
  dict.Set(kNtpCustomBackgroundURL, base::Value(kValidBackgroundUrl));
  dict.Set(kNtpCustomBackgroundAttributionLine1,
           base::Value(kAttributionLine1));
  dict.Set(kNtpCustomBackgroundAttributionLine2,
           base::Value(kAttributionLine2));
  dict.Set(kNtpCustomBackgroundAttributionActionURL,
           base::Value(kValidActionUrl));
  dict.Set(kNtpCustomBackgroundCollectionId, base::Value(kCollectionId));
  dict.Set(kNtpCustomBackgroundResumeToken, base::Value(kResumeToken));
  dict.Set(kNtpCustomBackgroundRefreshTimestamp, base::Value(123456789));
  dict.Set(kNtpCustomBackgroundMainColor, base::Value(16711680));

  sync_pb::NtpCustomBackground proto = GetProtoFromBackgroundDict(dict);

  EXPECT_EQ(proto.url(), kValidBackgroundUrl);
  EXPECT_EQ(proto.attribution_line_1(), kAttributionLine1);
  EXPECT_EQ(proto.attribution_line_2(), kAttributionLine2);
  EXPECT_EQ(proto.attribution_action_url(), kValidActionUrl);
  EXPECT_EQ(proto.collection_id(), kCollectionId);
  EXPECT_EQ(proto.resume_token(), kResumeToken);
  EXPECT_EQ(proto.refresh_timestamp_unix_epoch_seconds(), 123456789);
  EXPECT_EQ(proto.main_color(), 16711680u);
}

}  // namespace
}  // namespace themes
