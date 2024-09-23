// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_proto_utils.h"

#include <memory>

#include "base/base64.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

const char kStartUrl[] = "https://example.com/launchurl";
const char kAppName[] = "Test name";
const char kScope[] = "https://example.com/scope";
const char kUserPageOrdinal[] = "fake_user_page_ordinal";
const char kUserLaunchOrdinal[] = "fake_user_launch_ordinal";
const char kIconUrl[] = "https://example.com/icon.png";
const int kIconSizePx = 128;

}  // namespace

// Test that a M85 serialized proto parses correctly in the current version.
TEST(WebAppProtoUtilsTest, M85SpecificsProtoParse) {
  // Serialized in branch 4183 (M85) using:
  // sync_proto.set_launch_url(kStartUrl);
  // sync_proto.set_name(kAppName);
  // sync_proto.set_user_display_mode(sync_pb::WebAppSpecifics::BROWSER);
  // sync_pb::WebAppIconInfo* icon_info = sync_proto.add_icon_infos();
  // icon_info->set_url(kIconUrl);
  // sync_proto.SerializeAsString()
  const std::string serialized_proto = {
      10,  29,  104, 116, 116, 112, 115, 58,  47,  47,  101, 120, 97,
      109, 112, 108, 101, 46,  99,  111, 109, 47,  108, 97,  117, 110,
      99,  104, 117, 114, 108, 18,  9,   84,  101, 115, 116, 32,  110,
      97,  109, 101, 24,  1,   50,  30,  18,  28,  104, 116, 116, 112,
      115, 58,  47,  47,  101, 120, 97,  109, 112, 108, 101, 46,  99,
      111, 109, 47,  105, 99,  111, 110, 46,  112, 110, 103};

  // Parse the proto.
  sync_pb::WebAppSpecifics sync_proto;
  bool parsed = sync_proto.ParseFromString(serialized_proto);

  // Check the proto was parsed.
  ASSERT_TRUE(parsed);
  EXPECT_EQ(kStartUrl, sync_proto.start_url());
  EXPECT_EQ(kAppName, sync_proto.name());
  EXPECT_EQ(sync_pb::WebAppSpecifics::BROWSER,
            sync_proto.user_display_mode_default());
  ASSERT_EQ(1, sync_proto.icon_infos_size());
  EXPECT_EQ(kIconUrl, sync_proto.icon_infos(0).url());
  EXPECT_FALSE(sync_proto.icon_infos(0).has_purpose());

  // Check the fields can be parsed into the IconInfo struct.
  std::optional<std::vector<apps::IconInfo>> parsed_icon_infos =
      ParseAppIconInfos("WebAppProtoUtilsTest", sync_proto.icon_infos());
  ASSERT_TRUE(parsed_icon_infos.has_value());
  ASSERT_EQ(1u, parsed_icon_infos.value().size());
  EXPECT_EQ(kIconUrl, parsed_icon_infos.value()[0].url);
  // Missing `purpose` parsed as `kAny`.
  EXPECT_EQ(apps::IconInfo::Purpose::kAny,
            parsed_icon_infos.value()[0].purpose);

  // Check the proto can be stored on a web app struct.
  WebApp web_app("app_id");
  web_app.SetStartUrl(GURL(kStartUrl));
  web_app.SetManifestId(webapps::ManifestId(kStartUrl));
  web_app.SetSyncProto(sync_proto);
}

// Test that a minimal M85 proto (ie. only fields that would always be set in
// M85) is correctly parsed to a apps::IconInfo in the current
// Chromium version.
TEST(WebAppProtoUtilsTest, M85SpecificsProtoToWebApp_Minimal) {
  // Set the minimal proto fields.
  sync_pb::WebAppSpecifics sync_proto;
  sync_proto.set_start_url(kStartUrl);
  sync_proto.set_name(kAppName);
  sync_proto.set_user_display_mode_default(sync_pb::WebAppSpecifics::BROWSER);

  EXPECT_EQ(kAppName, sync_proto.name());

  // Check the fields were parsed into the IconInfo struct.
  std::optional<std::vector<apps::IconInfo>> parsed_icon_infos =
      ParseAppIconInfos("WebAppProtoUtilsTest", sync_proto.icon_infos());

  // Check the fields were parsed.
  ASSERT_TRUE(parsed_icon_infos.has_value());
  ASSERT_EQ(0u, parsed_icon_infos->size());

  // Check the proto can be stored on a web app struct.
  WebApp web_app("app_id");
  web_app.SetStartUrl(GURL(kStartUrl));
  web_app.SetManifestId(webapps::ManifestId(kStartUrl));
  web_app.SetSyncProto(sync_proto);
}

// Test that a M85 proto with all fields populated is correctly parsed to a
// apps::IconInfo in the current Chromium version.
TEST(WebAppProtoUtilsTest, M85SpecificsProtoToWebApp_FullyPopulated) {
  // Set all proto fields.
  sync_pb::WebAppSpecifics sync_proto;
  sync_proto.set_start_url(kStartUrl);
  sync_proto.set_name(kAppName);
  sync_proto.set_user_display_mode_default(
      sync_pb::WebAppSpecifics::STANDALONE);
  sync_proto.set_theme_color(SK_ColorRED);
  sync_proto.set_scope(kScope);
  sync_proto.set_user_page_ordinal(kUserPageOrdinal);
  sync_proto.set_user_launch_ordinal(kUserLaunchOrdinal);
  sync_pb::WebAppIconInfo* icon_info_1 = sync_proto.add_icon_infos();
  icon_info_1->set_url(kIconUrl);
  icon_info_1->set_size_in_px(kIconSizePx);
  icon_info_1->set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  sync_pb::WebAppIconInfo* icon_info_2 = sync_proto.add_icon_infos();
  icon_info_2->set_url(kIconUrl);
  icon_info_2->set_size_in_px(kIconSizePx);
  icon_info_2->set_purpose(sync_pb::WebAppIconInfo_Purpose_MASKABLE);

  // Check the fields can be parsed into the IconInfo struct.
  std::optional<std::vector<apps::IconInfo>> icon_infos =
      ParseAppIconInfos("WebAppProtoUtilsTest", sync_proto.icon_infos());
  ASSERT_TRUE(icon_infos.has_value());
  ASSERT_EQ(2u, icon_infos->size());
  EXPECT_EQ(kIconUrl, icon_infos.value()[0].url);
  EXPECT_EQ(kIconSizePx, icon_infos.value()[0].square_size_px);
  EXPECT_EQ(apps::IconInfo::Purpose::kAny, icon_infos.value()[0].purpose);
  EXPECT_EQ(kIconUrl, icon_infos.value()[1].url);
  EXPECT_EQ(kIconSizePx, icon_infos.value()[1].square_size_px);
  EXPECT_EQ(apps::IconInfo::Purpose::kMaskable, icon_infos.value()[1].purpose);
}

// Test that a serialized proto with an unrecognized new field parses correctly
// in the current version and preserves the field value.
TEST(WebAppProtoUtilsTest, SpecificsProtoWithNewFieldParses) {
  // Serialized in M125 by modifying web_app_specifics.proto:
  // +optional string test_new_field = 5372767;
  // Then:
  // sync_pb::WebAppSpecifics sync_proto;
  // sync_proto.set_start_url(kStartUrl);
  // sync_proto.set_name(kAppName);
  // sync_proto.set_user_display_mode_default(
  //     sync_pb::WebAppSpecifics::BROWSER);
  // sync_proto.set_test_new_field("hello");
  // sync_proto.SerializeAsString();
  const std::string serialized_proto = {
      10,  29,  104, 116, 116, 112, 115, 58,  47,  47,  101, 120, 97,  109,
      112, 108, 101, 46,  99,  111, 109, 47,  108, 97,  117, 110, 99,  104,
      117, 114, 108, 18,  9,   84,  101, 115, 116, 32,  110, 97,  109, 101,
      24,  1,   -6,  -75, -65, 20,  5,   104, 101, 108, 108, 111};

  // Parse the proto.
  sync_pb::WebAppSpecifics sync_proto;
  bool parsed = sync_proto.ParseFromString(serialized_proto);

  // Check the proto was parsed.
  ASSERT_TRUE(parsed);
  EXPECT_EQ(kStartUrl, sync_proto.start_url());

  // Check the proto can be stored on a web app struct.
  WebApp web_app("app_id");
  web_app.SetStartUrl(GURL(kStartUrl));
  web_app.SetManifestId(webapps::ManifestId(kStartUrl));
  web_app.SetSyncProto(sync_proto);

  // Clear the extra field added due to normalizing the proto in `SetSyncProto`.
  sync_pb::WebAppSpecifics result_proto = web_app.sync_proto();
  result_proto.clear_relative_manifest_id();

  // Check that the sync proto retained its value, including the unknown field.
  EXPECT_EQ(result_proto.SerializeAsString(), serialized_proto);
}

// Test that a serialized proto with an unrecognized new enum value parses
// correctly in the current version and preserves the enum value.
TEST(WebAppProtoUtilsTest, SpecificsProtoWithNewEnumValueParses) {
  // Serialized in M125 by modifying web_app_specifics.proto:
  // in "enum UserDisplayMode":
  // +TEST_NEW_ENUM = 5372767;
  // Then:
  // sync_pb::WebAppSpecifics sync_proto;
  // sync_proto.set_start_url(kStartUrl);
  // sync_proto.set_name(kAppName);
  // sync_proto.set_user_display_mode_default(
  //     sync_pb::WebAppSpecifics::TEST_NEW_ENUM);
  // sync_proto.SerializeAsString();
  const std::string serialized_proto = {
      10,  29,  104, 116, 116, 112, 115, 58,  47,  47,  101, 120,
      97,  109, 112, 108, 101, 46,  99,  111, 109, 47,  108, 97,
      117, 110, 99,  104, 117, 114, 108, 18,  9,   84,  101, 115,
      116, 32,  110, 97,  109, 101, 24,  -33, -10, -57, 2};

  // Parse the proto.
  sync_pb::WebAppSpecifics sync_proto;
  bool parsed = sync_proto.ParseFromString(serialized_proto);

  // Check the proto was parsed.
  ASSERT_TRUE(parsed);
  EXPECT_EQ(kStartUrl, sync_proto.start_url());
  // Unfortunately, proto2 reports unknown field values as unset.
  EXPECT_FALSE(sync_proto.has_user_display_mode_default());
  // Unknown field value parses as 0, ie. "Unspecified".
  EXPECT_EQ(sync_proto.user_display_mode_default(),
            sync_pb::WebAppSpecifics_UserDisplayMode_UNSPECIFIED);

  // Check the proto can be stored on a web app struct.
  WebApp web_app("app_id");
  web_app.SetStartUrl(GURL(kStartUrl));
  web_app.SetManifestId(webapps::ManifestId(kStartUrl));
  web_app.SetSyncProto(sync_proto);

  // Clear the extra field added due to normalizing the proto in `SetSyncProto`.
  sync_pb::WebAppSpecifics result_proto = web_app.sync_proto();
  result_proto.clear_relative_manifest_id();

  // Check that the sync proto retained its value, including the unknown field.
  EXPECT_EQ(result_proto.SerializeAsString(), serialized_proto);
}

TEST(WebAppProtoUtilsTest, RunOnOsLoginModes) {
  RunOnOsLoginMode mode = ToRunOnOsLoginMode(WebAppProto::MINIMIZED);
  EXPECT_EQ(RunOnOsLoginMode::kMinimized, mode);

  mode = ToRunOnOsLoginMode(WebAppProto::WINDOWED);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed, mode);

  mode = ToRunOnOsLoginMode(WebAppProto::NOT_RUN);
  EXPECT_EQ(RunOnOsLoginMode::kNotRun, mode);

  // Any other value should return kNotRun.
  mode = ToRunOnOsLoginMode(static_cast<WebAppProto::RunOnOsLoginMode>(0xCAFE));
  EXPECT_EQ(RunOnOsLoginMode::kNotRun, mode);

  WebAppProto::RunOnOsLoginMode proto_mode =
      ToWebAppProtoRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(WebAppProto::WINDOWED, proto_mode);

  proto_mode = ToWebAppProtoRunOnOsLoginMode(RunOnOsLoginMode::kMinimized);
  EXPECT_EQ(WebAppProto::MINIMIZED, proto_mode);

  proto_mode = ToWebAppProtoRunOnOsLoginMode(RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(WebAppProto::NOT_RUN, proto_mode);
}

}  // namespace web_app
