// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database_serialization.h"

#include <memory>
#include <string>

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

// Creates a protobuf web app that passes the parsing checks.
proto::WebApp CreateWebAppProtoForTesting(const std::string& name,
                                          const GURL& start_url) {
  proto::WebApp web_app;
  CHECK(start_url.is_valid());
  web_app.set_name(name);
  web_app.mutable_sync_data()->set_name(name);
  web_app.mutable_sync_data()->set_start_url(start_url.spec());
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  web_app.mutable_sync_data()->set_relative_manifest_id(
      RelativeManifestIdPath(manifest_id));
  web_app.mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
#if BUILDFLAG(IS_CHROMEOS)
  web_app.mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode::
          WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif
  web_app.set_scope(start_url.GetWithoutFilename().spec());
  web_app.mutable_sources()->set_user_installed(true);
#if BUILDFLAG(IS_CHROMEOS)
  web_app.mutable_chromeos_data();
#endif
  web_app.set_install_state(
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  return web_app;
}

using WebAppDatabaseSerializationTest = WebAppTest;

TEST_F(WebAppDatabaseSerializationTest, RandomWebApps) {
  // This tests attempts to ensure that the WebApp serialization code and
  // deserialization code is consistent when serialized back and forth from the
  // proto representation. The random web app generation should ensure that all
  // fields in all relevant combinations are tested.
  for (int i = 0; i < 1000; ++i) {
    std::unique_ptr<WebApp> app = test::CreateRandomWebApp(
        {.seed = static_cast<uint32_t>(i), .non_zero = i == 0});
    std::unique_ptr<proto::WebApp> proto = WebAppToProto(*app);
    std::unique_ptr<WebApp> parsed_app = ParseWebAppProto(*proto);
    ASSERT_THAT(parsed_app, NotNull());
    ASSERT_EQ(*app, *parsed_app);
    std::unique_ptr<proto::WebApp> round_trip_proto =
        WebAppToProto(*parsed_app);
    ASSERT_THAT(round_trip_proto, NotNull());
    ASSERT_EQ(proto->SerializeAsString(),
              round_trip_proto->SerializeAsString());
  }
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingSyncData) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_sync_data();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingStartUrl) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.mutable_sync_data()->clear_start_url();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidStartUrl) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.mutable_sync_data()->set_start_url("invalid-url");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_MissingRelativeManifestId) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  // Clear the field that should be populated by migration.
  proto.mutable_sync_data()->clear_relative_manifest_id();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingScope) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_scope();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidScope) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.set_scope("invalid-scope");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_ScopeWithQuery) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  // Set a scope with a query, which should have been removed by migration.
  proto.set_scope("https://example.com/path?query=1");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_ScopeWithRef) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  // Set a scope with a ref, which should have been removed by migration.
  proto.set_scope("https://example.com/path#ref");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_MissingUserDisplayMode) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.mutable_sync_data()->clear_user_display_mode_default();
#if BUILDFLAG(IS_CHROMEOS)
  proto.mutable_sync_data()->clear_user_display_mode_cros();
#endif
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_MissingCurrentPlatformUserDisplayMode) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
#if BUILDFLAG(IS_CHROMEOS)
  proto.mutable_sync_data()->clear_user_display_mode_cros();
  proto.mutable_sync_data()->set_user_display_mode_default(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
#else
  proto.mutable_sync_data()->clear_user_display_mode_default();
  proto.mutable_sync_data()->set_user_display_mode_cros(
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE);
#endif
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingSources) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_sources();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_EmptySources) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.mutable_sources()->Clear();  // Clears all source booleans
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingName) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_name();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingInstallState) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_install_state();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InconsistentOsIntegrationStateOk) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.set_install_state(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  proto.clear_current_os_integration_states();  // Remove OS state
  EXPECT_THAT(ParseWebAppProto(proto), NotNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  proto2.set_install_state(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  proto2.mutable_current_os_integration_states()
      ->clear_shortcut();  // Remove shortcut state specifically
  EXPECT_THAT(ParseWebAppProto(proto2), NotNull());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_MissingCrOSData) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.clear_chromeos_data();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}
#else
TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_HasCrOSData) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.mutable_chromeos_data();  // Add CrOS data on non-CrOS
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidFileHandler) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* handler = proto.add_file_handlers();
  // Missing action
  handler->set_launch_type(proto::WebAppFileHandler::LAUNCH_TYPE_SINGLE_CLIENT);
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* handler2 = proto2.add_file_handlers();
  handler2->set_action("https://example.com/handle");
  // Missing launch type
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());

  proto::WebApp proto3 =
      CreateWebAppProtoForTesting("Test App 3", GURL("https://example3.com/"));
  auto* handler3 = proto3.add_file_handlers();
  handler3->set_action("invalid-action");  // Invalid action URL
  handler3->set_launch_type(
      proto::WebAppFileHandler::LAUNCH_TYPE_SINGLE_CLIENT);
  EXPECT_THAT(ParseWebAppProto(proto3), IsNull());

  proto::WebApp proto4 =
      CreateWebAppProtoForTesting("Test App 4", GURL("https://example4.com/"));
  auto* handler4 = proto4.add_file_handlers();
  handler4->set_action("https://example.com/handle");
  handler4->set_launch_type(
      proto::WebAppFileHandler::LAUNCH_TYPE_SINGLE_CLIENT);
  auto* accept = handler4->add_accept();
  // Missing mimetype
  accept->add_file_extensions(".txt");
  EXPECT_THAT(ParseWebAppProto(proto4), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidShareTarget) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* target = proto.mutable_share_target();
  // Missing action
  target->set_method(proto::ShareTarget::METHOD_GET);
  target->set_enctype(proto::ShareTarget::ENCTYPE_FORM_URL_ENCODED);
  target->mutable_params();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* target2 = proto2.mutable_share_target();
  target2->set_action("invalid-action");  // Invalid action URL
  target2->set_method(proto::ShareTarget::METHOD_GET);
  target2->set_enctype(proto::ShareTarget::ENCTYPE_FORM_URL_ENCODED);
  target2->mutable_params();
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());

  proto::WebApp proto3 =
      CreateWebAppProtoForTesting("Test App 3", GURL("https://example3.com/"));
  auto* target3 = proto3.mutable_share_target();
  target3->set_action("https://example.com/share");
  target3->set_method(proto::ShareTarget::METHOD_POST);
  target3->set_enctype(proto::ShareTarget::ENCTYPE_MULTIPART_FORM_DATA);
  auto* params3 = target3->mutable_params();
  auto* file = params3->add_files();
  // Missing file name
  file->add_accept(".jpg");
  EXPECT_THAT(ParseWebAppProto(proto3), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidShortcut) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* shortcut = proto.add_shortcuts_menu_item_infos();
  // Missing name
  shortcut->set_url("https://example.com/shortcut");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* shortcut2 = proto2.add_shortcuts_menu_item_infos();
  shortcut2->set_name("Shortcut Name");
  // Missing URL
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_MismatchedShortcutSizes) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* shortcut = proto.add_shortcuts_menu_item_infos();
  shortcut->set_name("Shortcut Name");
  shortcut->set_url("https://example.com/shortcut");
  // Add one shortcut info but two downloaded size entries
  proto.add_downloaded_shortcuts_menu_icons_sizes();
  proto.add_downloaded_shortcuts_menu_icons_sizes();
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidProtocolHandler) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* handler = proto.add_protocol_handlers();
  // Missing protocol
  handler->set_url("https://example.com/handle?q=%s");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* handler2 = proto2.add_protocol_handlers();
  handler2->set_protocol("web+test");
  // Missing URL
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());

  proto::WebApp proto3 =
      CreateWebAppProtoForTesting("Test App 3", GURL("https://example3.com/"));
  auto* handler3 = proto3.add_protocol_handlers();
  handler3->set_protocol("web+test");
  handler3->set_url("invalid-url");  // Invalid URL
  EXPECT_THAT(ParseWebAppProto(proto3), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidScopeExtension) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* ext = proto.add_scope_extensions();
  // Missing origin
  ext->set_has_origin_wildcard(false);
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* ext2 = proto2.add_scope_extensions();
  ext2->set_origin("https://test.com");
  // Missing has_origin_wildcard
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());

  proto::WebApp proto3 =
      CreateWebAppProtoForTesting("Test App 3", GURL("https://example3.com/"));
  auto* ext3 = proto3.add_scope_extensions();
  ext3->set_origin("invalid-origin");  // Invalid origin
  ext3->set_has_origin_wildcard(false);
  EXPECT_THAT(ParseWebAppProto(proto3), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidValidatedScopeExtension) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* ext = proto.add_scope_extensions_validated();
  ext->set_origin("https://test.com");
  ext->set_scope("https://different.com");  // Different origin scope
  ext->set_has_origin_wildcard(false);
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest, ParseWebAppProto_InvalidManifestUrl) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  proto.set_manifest_url("invalid-url");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidExternalManagementConfig) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* config = proto.add_management_to_external_config_info();
  config->set_management(proto::WEB_APP_MANAGEMENT_TYPE_POLICY);
  config->set_is_placeholder(false);
  config->add_install_urls("invalid-url");  // Invalid install URL
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* config2 = proto2.add_management_to_external_config_info();
  config2->set_management(proto::WEB_APP_MANAGEMENT_TYPE_POLICY);
  config2->set_is_placeholder(false);
  config2->add_additional_policy_ids("");  // Empty policy ID
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());
}

// --- Tests for IsolationData ---

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_Version) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("invalid-version");  // Invalid version format
  // Set a valid location to isolate the version check
  isolation_data->mutable_proxy()->set_proxy_url("https://proxy.com");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_Location) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  // Set an invalid location (e.g., empty proxy URL)
  isolation_data->mutable_proxy()->set_proxy_url("");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());

  proto::WebApp proto2 =
      CreateWebAppProtoForTesting("Test App 2", GURL("https://example2.com/"));
  auto* isolation_data2 = proto2.mutable_isolation_data();
  isolation_data2->set_version("1.0.0");
  // Set an invalid location (non-ASCII owned bundle name)
  isolation_data2->mutable_owned_bundle()->set_dir_name_ascii("日本");
  EXPECT_THAT(ParseWebAppProto(proto2), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_PendingLocation) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  isolation_data->mutable_proxy()->set_proxy_url(
      "https://proxy.com");  // Valid current location

  auto* pending_info = isolation_data->mutable_pending_update_info();
  pending_info->set_version("2.0.0");
  // Set an invalid pending location
  pending_info->mutable_proxy()->set_proxy_url("invalid-url");
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_MismatchedDevMode) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  // Current location: Owned bundle, dev_mode = false
  isolation_data->mutable_owned_bundle()->set_dir_name_ascii("bundle");
  isolation_data->mutable_owned_bundle()->set_dev_mode(false);

  auto* pending_info = isolation_data->mutable_pending_update_info();
  pending_info->set_version("2.0.0");
  // Pending location: Owned bundle, dev_mode = true (mismatch)
  pending_info->mutable_owned_bundle()->set_dir_name_ascii("bundle_v2");
  pending_info->mutable_owned_bundle()->set_dev_mode(true);

  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_PendingVersion) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  isolation_data->mutable_proxy()->set_proxy_url(
      "https://proxy.com");  // Valid current location

  auto* pending_info = isolation_data->mutable_pending_update_info();
  pending_info->set_version("invalid-version");  // Invalid pending version
  // Set a valid pending location
  pending_info->mutable_proxy()->set_proxy_url("https://proxy-v2.com");

  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

// Note: Testing invalid integrity block data requires more setup/mocking.

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_UpdateManifestUrl) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  isolation_data->mutable_proxy()->set_proxy_url("https://proxy.com");
  isolation_data->set_update_manifest_url("invalid-url");  // Invalid URL
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidIsolationData_UpdateChannel) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* isolation_data = proto.mutable_isolation_data();
  isolation_data->set_version("1.0.0");
  isolation_data->mutable_proxy()->set_proxy_url("https://proxy.com");
  isolation_data->set_update_channel("");  // Invalid channel
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

// --- Tests for GeneratedIconFix ---

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidGeneratedIconFix_MissingSource) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* fix = proto.mutable_generated_icon_fix();
  // Missing source
  fix->set_window_start_time(syncer::TimeToProtoTime(base::Time::Now()));
  fix->set_attempt_count(1);
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidGeneratedIconFix_MissingWindowStart) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* fix = proto.mutable_generated_icon_fix();
  fix->set_source(
      proto::GeneratedIconFixSource::GENERATED_ICON_FIX_SOURCE_SYNC_INSTALL);
  // Missing window_start_time
  fix->set_attempt_count(1);
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

TEST_F(WebAppDatabaseSerializationTest,
       ParseWebAppProto_InvalidGeneratedIconFix_MissingAttemptCount) {
  proto::WebApp proto =
      CreateWebAppProtoForTesting("Test App", GURL("https://example.com/"));
  auto* fix = proto.mutable_generated_icon_fix();
  fix->set_source(
      proto::GeneratedIconFixSource::GENERATED_ICON_FIX_SOURCE_SYNC_INSTALL);
  fix->set_window_start_time(syncer::TimeToProtoTime(base::Time::Now()));
  // Missing attempt_count
  EXPECT_THAT(ParseWebAppProto(proto), IsNull());
}

}  // namespace
}  // namespace web_app
