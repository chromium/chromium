// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database_serialization.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_launch_handler.pb.h"
#include "chrome/browser/web_applications/proto/web_app_related_applications.pb.h"
#include "chrome/browser/web_applications/proto/web_app_share_target.pb.h"
#include "chrome/browser/web_applications/proto/web_app_tab_strip.pb.h"
#include "chrome/browser/web_applications/proto/web_app_url_pattern.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/441959098): Consider removing chromeos includes.
#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {

namespace {

// Records the result of parsing a WebApp protobuf object into a WebApp class.
void RecordProtoParseResult(ProtoParseResult result) {
  base::UmaHistogramEnumeration("WebAppProto.Parse.Result", result);
}

DisplayMode ToMojomDisplayMode(proto::WebApp::DisplayMode display_mode) {
  switch (display_mode) {
    case proto::WebApp::DISPLAY_MODE_UNSPECIFIED:
      return DisplayMode::kUndefined;
    case proto::WebApp::DISPLAY_MODE_BROWSER:
      return DisplayMode::kBrowser;
    case proto::WebApp::DISPLAY_MODE_MINIMAL_UI:
      return DisplayMode::kMinimalUi;
    case proto::WebApp::DISPLAY_MODE_STANDALONE:
      return DisplayMode::kStandalone;
    case proto::WebApp::DISPLAY_MODE_FULLSCREEN:
      return DisplayMode::kFullscreen;
    case proto::WebApp::DISPLAY_MODE_WINDOW_CONTROLS_OVERLAY:
      return DisplayMode::kWindowControlsOverlay;
    case proto::WebApp::DISPLAY_MODE_TABBED:
      return DisplayMode::kTabbed;
    case proto::WebApp::DISPLAY_MODE_BORDERLESS:
      return DisplayMode::kBorderless;
    case proto::WebApp::DISPLAY_MODE_PICTURE_IN_PICTURE:
      return DisplayMode::kPictureInPicture;
  }
}

proto::WebApp::DisplayMode ToWebAppProtoDisplayMode(DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return proto::WebApp::DISPLAY_MODE_BROWSER;
    case DisplayMode::kMinimalUi:
      return proto::WebApp::DISPLAY_MODE_MINIMAL_UI;
    case DisplayMode::kUndefined:
      NOTREACHED();
    case DisplayMode::kStandalone:
      return proto::WebApp::DISPLAY_MODE_STANDALONE;
    case DisplayMode::kFullscreen:
      return proto::WebApp::DISPLAY_MODE_FULLSCREEN;
    case DisplayMode::kWindowControlsOverlay:
      return proto::WebApp::DISPLAY_MODE_WINDOW_CONTROLS_OVERLAY;
    case DisplayMode::kTabbed:
      return proto::WebApp::DISPLAY_MODE_TABBED;
    case DisplayMode::kBorderless:
      return proto::WebApp::DISPLAY_MODE_BORDERLESS;
    case DisplayMode::kPictureInPicture:
      return proto::WebApp::DISPLAY_MODE_PICTURE_IN_PICTURE;
  }
}

proto::ShareTarget_Method MethodToProto(apps::ShareTarget::Method method) {
  switch (method) {
    case apps::ShareTarget::Method::kGet:
      return proto::ShareTarget::METHOD_GET;
    case apps::ShareTarget::Method::kPost:
      return proto::ShareTarget::METHOD_POST;
  }
}

apps::ShareTarget::Method ProtoToMethod(proto::ShareTarget_Method method) {
  switch (method) {
    case proto::ShareTarget::METHOD_GET:
      return apps::ShareTarget::Method::kGet;
    case proto::ShareTarget::METHOD_POST:
      return apps::ShareTarget::Method::kPost;
  }
}

proto::ShareTarget_Enctype EnctypeToProto(apps::ShareTarget::Enctype enctype) {
  switch (enctype) {
    case apps::ShareTarget::Enctype::kFormUrlEncoded:
      return proto::ShareTarget::ENCTYPE_FORM_URL_ENCODED;
    case apps::ShareTarget::Enctype::kMultipartFormData:
      return proto::ShareTarget::ENCTYPE_MULTIPART_FORM_DATA;
  }
}

apps::ShareTarget::Enctype ProtoToEnctype(proto::ShareTarget_Enctype enctype) {
  switch (enctype) {
    case proto::ShareTarget::ENCTYPE_FORM_URL_ENCODED:
      return apps::ShareTarget::Enctype::kFormUrlEncoded;
    case proto::ShareTarget::ENCTYPE_MULTIPART_FORM_DATA:
      return apps::ShareTarget::Enctype::kMultipartFormData;
  }
}

LaunchHandler ProtoToLaunchHandler(const proto::LaunchHandler& proto) {
  switch (proto.client_mode()) {
    case proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED:
      return LaunchHandler(std::nullopt);
    case proto::LaunchHandler::CLIENT_MODE_AUTO:
      return LaunchHandler(LaunchHandler::ClientMode::kAuto);
    case proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW:
      return LaunchHandler(LaunchHandler::ClientMode::kNavigateNew);
    case proto::LaunchHandler::CLIENT_MODE_NAVIGATE_EXISTING:
      return LaunchHandler(LaunchHandler::ClientMode::kNavigateExisting);
    case proto::LaunchHandler::CLIENT_MODE_FOCUS_EXISTING:
      return LaunchHandler(LaunchHandler::ClientMode::kFocusExisting);
    default:
      // Because proto deserialization populates the enum from an 'int'
      // without bounds checking, we need to handle the default case.
      return LaunchHandler(std::nullopt);
  }
}

proto::LaunchHandler LaunchHandlerToProto(LaunchHandler launch_handler) {
  proto::LaunchHandler proto_launch_handler;
  if (!launch_handler.client_mode_valid_and_specified()) {
    proto_launch_handler.set_client_mode(
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
    return proto_launch_handler;
  }
  proto::LaunchHandler::ClientMode client_mode =
      proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED;
  switch (launch_handler.parsed_client_mode()) {
    case LaunchHandler::ClientMode::kAuto:
      client_mode = proto::LaunchHandler::CLIENT_MODE_AUTO;
      break;
    case LaunchHandler::ClientMode::kNavigateNew:
      client_mode = proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW;
      break;
    case LaunchHandler::ClientMode::kNavigateExisting:
      client_mode = proto::LaunchHandler::CLIENT_MODE_NAVIGATE_EXISTING;
      break;
    case LaunchHandler::ClientMode::kFocusExisting:
      client_mode = proto::LaunchHandler::CLIENT_MODE_FOCUS_EXISTING;
      break;
  }
  proto_launch_handler.set_client_mode(client_mode);
  return proto_launch_handler;
}

ApiApprovalState ProtoToApiApprovalState(
    proto::WebApp::ApiApprovalState approval_state) {
  switch (approval_state) {
    case proto::WebApp_ApiApprovalState_REQUIRES_PROMPT:
      return ApiApprovalState::kRequiresPrompt;
    case proto::WebApp_ApiApprovalState_ALLOWED:
      return ApiApprovalState::kAllowed;
    case proto::WebApp_ApiApprovalState_DISALLOWED:
      return ApiApprovalState::kDisallowed;
  }
}

proto::WebApp::ApiApprovalState ApiApprovalStateToProto(
    ApiApprovalState approval_state) {
  switch (approval_state) {
    case ApiApprovalState::kRequiresPrompt:
      return proto::WebApp_ApiApprovalState_REQUIRES_PROMPT;
    case ApiApprovalState::kAllowed:
      return proto::WebApp_ApiApprovalState_ALLOWED;
    case ApiApprovalState::kDisallowed:
      return proto::WebApp_ApiApprovalState_DISALLOWED;
  }
}

apps::FileHandler::LaunchType ProtoToLaunchType(
    proto::WebAppFileHandler::LaunchType state) {
  switch (state) {
    case proto::WebAppFileHandler::LAUNCH_TYPE_SINGLE_CLIENT:
      return apps::FileHandler::LaunchType::kSingleClient;
    case proto::WebAppFileHandler::LAUNCH_TYPE_MULTIPLE_CLIENTS:
      return apps::FileHandler::LaunchType::kMultipleClients;
    case proto::WebAppFileHandler::LAUNCH_TYPE_UNSPECIFIED:
      return apps::FileHandler::LaunchType::kSingleClient;
  }
}

proto::WebAppFileHandler::LaunchType LaunchTypeToProto(
    apps::FileHandler::LaunchType state) {
  switch (state) {
    case apps::FileHandler::LaunchType::kSingleClient:
      return proto::WebAppFileHandler::LAUNCH_TYPE_SINGLE_CLIENT;
    case apps::FileHandler::LaunchType::kMultipleClients:
      return proto::WebAppFileHandler::LAUNCH_TYPE_MULTIPLE_CLIENTS;
  }
}

WebAppManagement::Type ProtoToWebAppManagement(
    proto::WebAppManagementType type) {
  switch (type) {
    case proto::WEB_APP_MANAGEMENT_TYPE_UNSPECIFIED:
      NOTREACHED();
    case proto::WEB_APP_MANAGEMENT_TYPE_SYSTEM:
      return WebAppManagement::Type::kSystem;
    case proto::WEB_APP_MANAGEMENT_TYPE_KIOSK:
      return WebAppManagement::Type::kKiosk;
    case proto::WEB_APP_MANAGEMENT_TYPE_POLICY:
      return WebAppManagement::Type::kPolicy;
    case proto::WEB_APP_MANAGEMENT_TYPE_SUB_APP:
      return WebAppManagement::Type::kSubApp;
    case proto::WEB_APP_MANAGEMENT_TYPE_WEB_APP_STORE:
      return WebAppManagement::Type::kWebAppStore;
    case proto::WEB_APP_MANAGEMENT_TYPE_SYNC:
      return WebAppManagement::Type::kSync;
    case proto::WEB_APP_MANAGEMENT_TYPE_USER_INSTALLED:
      return WebAppManagement::Type::kUserInstalled;
    case proto::WEB_APP_MANAGEMENT_TYPE_DEFAULT:
      return WebAppManagement::Type::kDefault;
    case proto::WEB_APP_MANAGEMENT_TYPE_IWA_SHIMLESS_RMA:
      return WebAppManagement::Type::kIwaShimlessRma;
    case proto::WEB_APP_MANAGEMENT_TYPE_IWA_POLICY:
      return WebAppManagement::Type::kIwaPolicy;
    case proto::WEB_APP_MANAGEMENT_TYPE_IWA_USER_INSTALLED:
      return WebAppManagement::Type::kIwaUserInstalled;
    case proto::WEB_APP_MANAGEMENT_TYPE_OEM:
      return WebAppManagement::Type::kOem;
    case proto::WEB_APP_MANAGEMENT_TYPE_ONE_DRIVE_INTEGRATION:
      return WebAppManagement::Type::kOneDriveIntegration;
    case proto::WEB_APP_MANAGEMENT_TYPE_APS_DEFAULT:
      return WebAppManagement::Type::kApsDefault;
  }
}

proto::WebAppManagementType WebAppManagementToProto(
    WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kSystem:
      return proto::WEB_APP_MANAGEMENT_TYPE_SYSTEM;
    case WebAppManagement::Type::kKiosk:
      return proto::WEB_APP_MANAGEMENT_TYPE_KIOSK;
    case WebAppManagement::Type::kPolicy:
      return proto::WEB_APP_MANAGEMENT_TYPE_POLICY;
    case WebAppManagement::Type::kSubApp:
      return proto::WEB_APP_MANAGEMENT_TYPE_SUB_APP;
    case WebAppManagement::Type::kWebAppStore:
      return proto::WEB_APP_MANAGEMENT_TYPE_WEB_APP_STORE;
    case WebAppManagement::Type::kSync:
      return proto::WEB_APP_MANAGEMENT_TYPE_SYNC;
    case WebAppManagement::Type::kUserInstalled:
      return proto::WEB_APP_MANAGEMENT_TYPE_USER_INSTALLED;
    case WebAppManagement::Type::kDefault:
      return proto::WEB_APP_MANAGEMENT_TYPE_DEFAULT;
    case WebAppManagement::Type::kIwaShimlessRma:
      return proto::WEB_APP_MANAGEMENT_TYPE_IWA_SHIMLESS_RMA;
    case WebAppManagement::Type::kIwaPolicy:
      return proto::WEB_APP_MANAGEMENT_TYPE_IWA_POLICY;
    case WebAppManagement::Type::kIwaUserInstalled:
      return proto::WEB_APP_MANAGEMENT_TYPE_IWA_USER_INSTALLED;
    case WebAppManagement::Type::kOem:
      return proto::WEB_APP_MANAGEMENT_TYPE_OEM;
    case WebAppManagement::Type::kOneDriveIntegration:
      return proto::WEB_APP_MANAGEMENT_TYPE_ONE_DRIVE_INTEGRATION;
    case WebAppManagement::Type::kApsDefault:
      return proto::WEB_APP_MANAGEMENT_TYPE_APS_DEFAULT;
  }
}

proto::TabStrip::Visibility TabStripVisibilityToProto(
    TabStrip::Visibility visibility) {
  switch (visibility) {
    case TabStrip::Visibility::kAuto:
      return proto::TabStrip::VISIBILITY_AUTO;
    case TabStrip::Visibility::kAbsent:
      return proto::TabStrip::VISIBILITY_ABSENT;
  }
}

std::string FilePathToProto(const base::FilePath& path) {
  base::Pickle pickle;
  path.WriteToPickle(&pickle);
  return std::string(pickle.data_as_char(), pickle.size());
}

std::optional<base::FilePath> ProtoToFilePath(const std::string& bytes) {
  const base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(bytes));
  base::PickleIterator pickle_iterator(pickle);

  base::FilePath path;
  if (!path.ReadFromPickle(&pickle_iterator)) {
    return std::nullopt;
  }
  return path;
}

template <typename T>
void IsolationDataLocationToProto(const IsolatedWebAppStorageLocation& location,
                                  T* proto) {
  std::visit(
      absl::Overload{
          [&proto](const IwaStorageOwnedBundle& bundle) {
            proto->mutable_owned_bundle()->set_dir_name_ascii(
                bundle.dir_name_ascii());
            proto->mutable_owned_bundle()->set_dev_mode(bundle.dev_mode());
          },
          [&proto](const IwaStorageUnownedBundle& bundle) {
            proto->mutable_unowned_bundle()->set_path(
                FilePathToProto(bundle.path()));
          },
          [&proto](const IwaStorageProxy& proxy) {
            DCHECK(!proxy.proxy_url().opaque());
            proto->mutable_proxy()->set_proxy_url(
                proxy.proxy_url().Serialize());
          },
      },
      location.variant());
}
template <typename T>
base::expected<IsolatedWebAppStorageLocation, std::string>
ProtoToIsolationDataLocation(const T& proto) {
  switch (proto.location_case()) {
    case T::LocationCase::kOwnedBundle: {
      std::string folder_name = proto.owned_bundle().dir_name_ascii();
      if (!base::IsStringASCII(folder_name)) {
        return base::unexpected(
            ".owned_bundle.dir_name_ascii parse error: cannot "
            "deserialize directory name");
      }
      return IwaStorageOwnedBundle{folder_name,
                                   proto.owned_bundle().dev_mode()};
    }
    case T::LocationCase::kUnownedBundle: {
      std::optional<base::FilePath> path =
          ProtoToFilePath(proto.unowned_bundle().path());
      if (!path.has_value()) {
        return base::unexpected(
            ".unowned_bundle.path parse error: cannot deserialize file path");
      }
      return IwaStorageUnownedBundle{*path};
    }
    case T::LocationCase::kProxy: {
      GURL gurl_proxy_url = GURL(proto.proxy().proxy_url());
      url::Origin proxy_url = url::Origin::Create(gurl_proxy_url);
      if (!gurl_proxy_url.is_valid() || proxy_url.opaque()) {
        return base::unexpected(
            ".proxy.proxy_url parse error: cannot deserialize proxy "
            "url. Value: " +
            proto.proxy().proxy_url());
      }
      return IwaStorageProxy{proxy_url};
    }
    case T::LocationCase::LOCATION_NOT_SET:
      return base::unexpected(" parse error: not set");
  }
}

}  // anonymous namespace

std::unique_ptr<WebApp> ParseWebAppProtoForTesting(  // IN-TEST
    const webapps::AppId& app_id,
    const std::string& value) {
  proto::WebApp proto;
  const bool parsed = proto.ParseFromString(value);
  if (!parsed) {
    DLOG(ERROR) << "WebApps LevelDB parse error: can't parse proto.";
    return nullptr;
  }

  auto web_app = ParseWebAppProto(proto);
  if (!web_app) {
    // ParseWebAppProto() already logged what went wrong here.
    return nullptr;
  }

  if (web_app->app_id() != app_id) {
    DLOG(ERROR) << "WebApps LevelDB error: app_id doesn't match storage key "
                << app_id << " vs " << web_app->app_id() << ", from "
                << web_app->manifest_id();
    return nullptr;
  }

  return web_app;
}

// Converts a WebApp protobuf into a WebApp object. Failure and success cases
// are measured via histograms.
std::unique_ptr<WebApp> ParseWebAppProto(const proto::WebApp& proto) {
  if (!proto.has_sync_data()) {
    RecordProtoParseResult(ProtoParseResult::kNoSyncData);
    DLOG(ERROR) << "WebApp proto parse error: no sync_data field";
    return nullptr;
  }

  const sync_pb::WebAppSpecifics& sync_data = proto.sync_data();

  if (!sync_data.has_start_url()) {
    RecordProtoParseResult(ProtoParseResult::kNoStartUrlInSyncData);
    DLOG(ERROR) << "WebApp proto start_url parse error: no start_url field";
    return nullptr;
  }

  GURL start_url(sync_data.start_url());
  if (start_url.is_empty() || !start_url.is_valid()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidStartUrl);
    DLOG(ERROR) << "WebApp proto start_url parse error: "
                << start_url.possibly_invalid_spec();
    return nullptr;
  }

  // Post-migration check: Scope should not be empty.
  if (!proto.has_scope() || proto.scope().empty()) {
    RecordProtoParseResult(ProtoParseResult::kNoScope);
    DLOG(ERROR) << "WebApp proto parse error: scope is empty.";
    return nullptr;
  }
  GURL scope(proto.scope());
  if (!scope.is_valid()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidScope);
    DLOG(ERROR) << "WebApp proto scope parse error: "
                << scope.possibly_invalid_spec();
    return nullptr;
  }
  if (scope.has_ref()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidScopeWithRef);
    DLOG(ERROR) << "WebApp proto has ref: " << scope.possibly_invalid_spec();
    return nullptr;
  }
  if (scope.has_query()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidScopeWithQuery);
    DLOG(ERROR) << "WebApp proto has query: " << scope.possibly_invalid_spec();
    return nullptr;
  }

  if (!sync_data.has_relative_manifest_id()) {
    RecordProtoParseResult(ProtoParseResult::kNoRelativeManifestId);
    DLOG(ERROR) << "WebApp proto parse error: no relative_manifest_id field.";
    return nullptr;
  }
  webapps::ManifestId manifest_id =
      GenerateManifestId(sync_data.relative_manifest_id(), start_url);
  if (!manifest_id.is_valid()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidManifestId);
    DLOG(ERROR) << "WebApp proto manifest_id parse error: cannot generate "
                   "valid manifest id from relative_manifest_id: "
                << sync_data.relative_manifest_id()
                << " and start_url: " << start_url.spec();
    return nullptr;
  }

  webapps::AppId app_id = GenerateAppIdFromManifestId(manifest_id);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->SetManifestId(manifest_id);
  // Set the sync proto early, as other setters might depend on it.
  web_app->SetSyncProto(sync_data);
  web_app->SetScope(scope);

  if (!sync_data.has_user_display_mode_cros() &&
      !sync_data.has_user_display_mode_default()) {
    RecordProtoParseResult(ProtoParseResult::kNoUserDisplayModeInSync);
    DLOG(ERROR) << "WebApp proto parse error: no user_display_mode field";
    return nullptr;
  }

  // Post-migration check: Ensure current platform UDM is set.
  if (!HasCurrentPlatformUserDisplayMode(sync_data)) {
    RecordProtoParseResult(
        ProtoParseResult::kMissingUserDisplayModeForCurrentPlatform);
    DLOG(ERROR) << "WebApp proto parse error: missing user display mode for "
                   "current platform";
    return nullptr;
  }

  // GenerateManifestId functions above strip the fragment part from the URL,
  // but stored sync data may still have a fragment in relative_manifest_id.
  // Per manifest spec, manifest IDs should be compared ignoring the fragment,
  // so we should remove it from the sync data. Note this doesn't trigger a DB
  // write or sync change - they will only happen if the app data changes for
  // some other reason (eg. launch).
  std::string relative_manifest_id_path = RelativeManifestIdPath(manifest_id);
  if (sync_data.has_relative_manifest_id() &&
      sync_data.relative_manifest_id() != relative_manifest_id_path) {
    auto modified_sync_data = sync_data;
    modified_sync_data.set_relative_manifest_id(relative_manifest_id_path);
    web_app->SetSyncProto(modified_sync_data);
    // Record when this happens. When it is rare enough we could simplify the
    // logic here by just treating apps with mismatching IDs as a parse error.
    base::UmaHistogramBoolean("WebApp.ParseWebAppProto.ManifestIdMatch", false);
  } else {
    web_app->SetSyncProto(sync_data);
    // Record success for comparison.
    base::UmaHistogramBoolean("WebApp.ParseWebAppProto.ManifestIdMatch", true);
  }

  // Required fields:
  if (!proto.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    RecordProtoParseResult(ProtoParseResult::kNoSources);
    return nullptr;
  }

  WebAppManagementTypes sources;
  sources.PutOrRemove(WebAppManagement::kSystem, proto.sources().system());
  sources.PutOrRemove(WebAppManagement::kPolicy, proto.sources().policy());
  sources.PutOrRemove(WebAppManagement::kWebAppStore,
                      proto.sources().web_app_store());
  sources.PutOrRemove(WebAppManagement::kSync, proto.sources().sync());
  sources.PutOrRemove(WebAppManagement::kUserInstalled,
                      proto.sources().user_installed());
  sources.PutOrRemove(WebAppManagement::kDefault, proto.sources().default_());
  sources.PutOrRemove(WebAppManagement::kOem, proto.sources().oem());
  sources.PutOrRemove(WebAppManagement::kSubApp, proto.sources().sub_app());
  sources.PutOrRemove(WebAppManagement::kKiosk, proto.sources().kiosk());
  sources.PutOrRemove(WebAppManagement::kIwaShimlessRma,
                      proto.sources().iwa_shimless_rma());
  sources.PutOrRemove(WebAppManagement::kIwaPolicy,
                      proto.sources().iwa_policy());
  sources.PutOrRemove(WebAppManagement::kIwaUserInstalled,
                      proto.sources().iwa_user_installed());
  sources.PutOrRemove(WebAppManagement::kOneDriveIntegration,
                      proto.sources().one_drive_integration());
  sources.PutOrRemove(WebAppManagement::kApsDefault,
                      proto.sources().aps_default());

  if (sources.empty() && !proto.is_uninstalling()) {
    RecordProtoParseResult(ProtoParseResult::kNoSourcesAndNotUninstalling);
    DLOG(ERROR) << "WebApp proto parse error: no source in sources field, "
                   "and is_uninstalling isn't true.";
    return nullptr;
  }
  web_app->sources_ = sources;

  if (!proto.has_name()) {
    RecordProtoParseResult(ProtoParseResult::kNoName);
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(proto.name());

  if (!proto.has_install_state()) {
    RecordProtoParseResult(ProtoParseResult::kNoInstallState);
    DLOG(ERROR) << "WebApp proto parse error: no install_state field";
    return nullptr;
  }
  if (!proto::InstallState_IsValid(proto.install_state())) {
    RecordProtoParseResult(ProtoParseResult::kInvalidInstallState);
    DLOG(ERROR) << "WebApp proto parse error: invalid install_state field: "
                << proto.install_state();
    return nullptr;
  }
  web_app->SetInstallState(proto.install_state());

  // Because the OS integration current state is saved in a two-phase-commit
  // flow, where the app is saved to the database first without os integration,
  // and then after the desired integration is complete the current os
  // integration state is saved, we don't reject parsing apps where the
  // install_state is INSTALLED_WITH_OS_INTEGRATION but the current os
  // integration state is not set.
  // This is handled in
  // MaybeInstallAppsFromSyncAndPendingInstallOrSyncOsIntegration.

  auto& chromeos_data_proto = proto.chromeos_data();

  if (IsChromeOsDataMandatory() && !proto.has_chromeos_data()) {
    RecordProtoParseResult(ProtoParseResult::kMissingChromeOsData);
    DLOG(ERROR) << "WebApp proto parse error: no chromeos_data field. The web "
                << "app might have been installed when running on an OS other "
                << "than Chrome OS.";
    return nullptr;
  }

  if (!IsChromeOsDataMandatory() && proto.has_chromeos_data()) {
    RecordProtoParseResult(ProtoParseResult::kHasChromeOsDataOnNonChromeOs);
    DLOG(ERROR) << "WebApp proto parse error: has chromeos_data field. The web "
                << "app might have been installed when running on Chrome OS.";
    return nullptr;
  }

  if (proto.has_chromeos_data()) {
    auto chromeos_data = std::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = chromeos_data_proto.show_in_launcher();
    chromeos_data->show_in_search_and_shelf =
        chromeos_data_proto.show_in_search_and_shelf();
    chromeos_data->show_in_management =
        chromeos_data_proto.show_in_management();
    chromeos_data->is_disabled = chromeos_data_proto.is_disabled();
    chromeos_data->oem_installed = chromeos_data_proto.oem_installed();
    chromeos_data->handles_file_open_intents =
        chromeos_data_proto.handles_file_open_intents();
    web_app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (proto.client_data().has_system_web_app_data()) {
    ash::SystemWebAppData& swa_data =
        web_app->client_data()->system_web_app_data.emplace();

    swa_data.system_app_type = static_cast<ash::SystemWebAppType>(
        proto.client_data().system_web_app_data().system_app_type());
  }
#endif

  // Optional fields:
  if (proto.has_launch_query_params()) {
    web_app->SetLaunchQueryParams(proto.launch_query_params());
  }

  if (proto.has_display_mode()) {
    web_app->SetDisplayMode(ToMojomDisplayMode(proto.display_mode()));
  }

  std::vector<DisplayMode> display_mode_override;
  for (int i = 0; i < proto.display_mode_override_size(); i++) {
    proto::WebApp::DisplayMode display_mode = proto.display_mode_override(i);
    display_mode_override.push_back(ToMojomDisplayMode(display_mode));
  }
  web_app->SetDisplayModeOverride(std::move(display_mode_override));

  if (proto.has_description()) {
    web_app->SetDescription(proto.description());
  }

  if (proto.has_theme_color()) {
    web_app->SetThemeColor(proto.theme_color());
  }

  if (proto.has_dark_mode_theme_color()) {
    web_app->SetDarkModeThemeColor(proto.dark_mode_theme_color());
  }

  if (proto.has_background_color()) {
    web_app->SetBackgroundColor(proto.background_color());
  }

  if (proto.has_dark_mode_background_color()) {
    web_app->SetDarkModeBackgroundColor(proto.dark_mode_background_color());
  }

  if (proto.has_is_from_sync_and_pending_installation()) {
    web_app->SetIsFromSyncAndPendingInstallation(
        proto.is_from_sync_and_pending_installation());
  }

  if (proto.has_is_uninstalling()) {
    web_app->SetIsUninstalling(proto.is_uninstalling());
  }

  if (proto.has_last_badging_time()) {
    web_app->SetLastBadgingTime(
        syncer::ProtoTimeToTime(proto.last_badging_time()));
  }
  if (proto.has_last_launch_time()) {
    web_app->SetLastLaunchTime(
        syncer::ProtoTimeToTime(proto.last_launch_time()));
  }
  if (proto.has_latest_install_source()) {
    int install_source = proto.latest_install_source();
    if (install_source >= 0 &&
        install_source <=
            static_cast<int>(webapps::WebappInstallSource::kMaxValue)) {
      web_app->SetLatestInstallSource(
          static_cast<webapps::WebappInstallSource>(install_source));
    }
  }
  if (proto.has_manifest_update_time()) {
    web_app->SetManifestUpdateTime(
        syncer::ProtoTimeToTime(proto.manifest_update_time()));
  }

  if (proto.has_first_install_time()) {
    web_app->SetFirstInstallTime(
        syncer::ProtoTimeToTime(proto.first_install_time()));
  }

  std::optional<std::vector<apps::IconInfo>> parsed_manifest_icons =
      ParseAppIconInfos("WebApp", proto.manifest_icons());
  if (!parsed_manifest_icons) {
    RecordProtoParseResult(ProtoParseResult::kNoManifestIcons);
    // ParseWebAppIconInfos() reports any errors.
    return nullptr;
  }
  web_app->SetManifestIcons(std::move(parsed_manifest_icons.value()));

  std::vector<SquareSizePx> icon_sizes_any;
  for (int32_t size : proto.downloaded_icon_sizes_purpose_any()) {
    icon_sizes_any.push_back(size);
  }
  web_app->SetDownloadedIconSizes(IconPurpose::ANY,
                                  SortedSizesPx(std::move(icon_sizes_any)));

  std::vector<SquareSizePx> icon_sizes_maskable;
  for (int32_t size : proto.downloaded_icon_sizes_purpose_maskable()) {
    icon_sizes_maskable.push_back(size);
  }
  web_app->SetDownloadedIconSizes(
      IconPurpose::MASKABLE, SortedSizesPx(std::move(icon_sizes_maskable)));

  std::vector<SquareSizePx> icon_sizes_monochrome;
  for (int32_t size : proto.downloaded_icon_sizes_purpose_monochrome()) {
    icon_sizes_monochrome.push_back(size);
  }
  web_app->SetDownloadedIconSizes(
      IconPurpose::MONOCHROME, SortedSizesPx(std::move(icon_sizes_monochrome)));

  web_app->SetIsGeneratedIcon(proto.is_generated_icon());

  apps::FileHandlers file_handlers;
  for (const auto& file_handler_proto : proto.file_handlers()) {
    if (!file_handler_proto.has_action() ||
        !file_handler_proto.has_launch_type()) {
      RecordProtoParseResult(
          ProtoParseResult::kInvalidFileHandlerNoActionOrLaunchType);
      DLOG(ERROR) << "WebApp FileHandler proto parse error";
      return nullptr;
    }
    apps::FileHandler file_handler;
    file_handler.action = GURL(file_handler_proto.action());

    if (file_handler.action.is_empty() || !file_handler.action.is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidFileHandlerAction);
      DLOG(ERROR) << "WebApp FileHandler proto action parse error";
      return nullptr;
    }

    if (file_handler_proto.has_display_name()) {
      file_handler.display_name =
          base::UTF8ToUTF16(file_handler_proto.display_name());
    }

    file_handler.launch_type =
        ProtoToLaunchType(file_handler_proto.launch_type());

    for (const auto& accept_entry_proto : file_handler_proto.accept()) {
      if (!accept_entry_proto.has_mimetype()) {
        RecordProtoParseResult(
            ProtoParseResult::kInvalidFileHandlerAcceptEntry);
        DLOG(ERROR) << "WebApp FileHandler proto parse error for "
                    << file_handler.action;
        return nullptr;
      }
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = accept_entry_proto.mimetype();
      for (const auto& file_extension : accept_entry_proto.file_extensions()) {
        if (base::Contains(accept_entry.file_extensions, file_extension)) {
          // We intentionally don't return a nullptr here; instead, duplicate
          // entries are absorbed.
          DLOG(ERROR) << "apps::FileHandler::AcceptEntry parsing encountered "
                      << "duplicate file extension";
        }
        accept_entry.file_extensions.insert(file_extension);
      }
      file_handler.accept.push_back(std::move(accept_entry));
    }

    std::optional<std::vector<apps::IconInfo>> file_handler_icon_infos =
        ParseAppIconInfos("WebApp", file_handler_proto.downloaded_icons());
    if (!file_handler_icon_infos) {
      RecordProtoParseResult(ProtoParseResult::kInvalidIconsInFileHandler);
      // ParseAppIconInfos() reports any errors.
      return nullptr;
    }
    file_handler.downloaded_icons = std::move(file_handler_icon_infos.value());

    file_handlers.push_back(std::move(file_handler));
  }
  web_app->SetFileHandlers(std::move(file_handlers));

  if (proto.has_share_target()) {
    const proto::ShareTarget& local_share_target = proto.share_target();
    if (!local_share_target.has_action() || !local_share_target.has_method() ||
        !local_share_target.has_enctype() || !local_share_target.has_params()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidShareTarget);
      DLOG(ERROR) << "WebApp proto Share Target parse error";
      return nullptr;
    }
    apps::ShareTarget share_target;

    const proto::ShareTargetParams& local_share_target_params =
        local_share_target.params();

    GURL action(local_share_target.action());
    if (action.is_empty() || !action.is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidShareTargetAction);
      DLOG(ERROR) << "WebApp proto action parse error: "
                  << action.possibly_invalid_spec();
      return nullptr;
    }

    share_target.action = action;
    share_target.method = ProtoToMethod(local_share_target.method());
    share_target.enctype = ProtoToEnctype(local_share_target.enctype());

    if (local_share_target_params.has_title()) {
      share_target.params.title = local_share_target_params.title();
    }
    if (local_share_target_params.has_text()) {
      share_target.params.text = local_share_target_params.text();
    }
    if (local_share_target_params.has_url()) {
      share_target.params.url = local_share_target_params.url();
    }

    for (const auto& share_target_params_file :
         local_share_target_params.files()) {
      if (!share_target_params_file.has_name()) {
        RecordProtoParseResult(ProtoParseResult::kInvalidShareTargetFile);
        DLOG(ERROR) << "WebApp proto Share Target files parse error for "
                    << share_target.action;
        return nullptr;
      }
      apps::ShareTarget::Files files_entry;
      files_entry.name = share_target_params_file.name();
      for (const auto& file_type : share_target_params_file.accept()) {
        if (base::Contains(files_entry.accept, file_type)) {
          // We intentionally don't return a nullptr here; instead, duplicate
          // entries are absorbed.
          DLOG(ERROR) << "apps::ShareTarget::Files parsing encountered "
                      << "duplicate file type";
        } else {
          files_entry.accept.push_back(file_type);
        }
      }
      share_target.params.files.push_back(std::move(files_entry));
    }

    web_app->SetShareTarget(std::move(share_target));
  }

  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos;
  for (const auto& shortcut_info_proto : proto.shortcuts_menu_item_infos()) {
    if (!shortcut_info_proto.has_name() || !shortcut_info_proto.has_url()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidShortcutsMenuItemInfo);
      DLOG(ERROR) << "WebApp proto Shortcut Menu Item Info parse error";
      return nullptr;
    }
    WebAppShortcutsMenuItemInfo shortcut_info;
    shortcut_info.name = base::UTF8ToUTF16(shortcut_info_proto.name());
    shortcut_info.url = GURL(shortcut_info_proto.url());
    for (IconPurpose purpose : kIconPurposes) {
      // This default init needed to infer the sophisticated protobuf type.
      const auto* shortcut_manifest_icons =
          &shortcut_info_proto.shortcut_manifest_icons();

      switch (purpose) {
        case IconPurpose::ANY:
          shortcut_manifest_icons =
              &shortcut_info_proto.shortcut_manifest_icons();
          break;
        case IconPurpose::MASKABLE:
          shortcut_manifest_icons =
              &shortcut_info_proto.shortcut_manifest_icons_maskable();
          break;
        case IconPurpose::MONOCHROME:
          shortcut_manifest_icons =
              &shortcut_info_proto.shortcut_manifest_icons_monochrome();
          break;
      }

      std::vector<WebAppShortcutsMenuItemInfo::Icon> manifest_icons;
      for (const auto& icon_info_proto : *shortcut_manifest_icons) {
        WebAppShortcutsMenuItemInfo::Icon shortcut_icon_info;
        shortcut_icon_info.square_size_px = icon_info_proto.size_in_px();
        shortcut_icon_info.url = GURL(icon_info_proto.url());
        manifest_icons.emplace_back(std::move(shortcut_icon_info));
      }
      shortcut_info.SetShortcutIconInfosForPurpose(purpose,
                                                   std::move(manifest_icons));
    }
    shortcuts_menu_item_infos.emplace_back(std::move(shortcut_info));
  }
  const size_t shortcut_menu_item_size = shortcuts_menu_item_infos.size();

  std::vector<IconSizes> shortcuts_menu_icons_sizes;
  for (const auto& shortcuts_icon_sizes_proto :
       proto.downloaded_shortcuts_menu_icons_sizes()) {
    IconSizes icon_sizes;
    icon_sizes.SetSizesForPurpose(
        IconPurpose::ANY, std::vector<SquareSizePx>(
                              shortcuts_icon_sizes_proto.icon_sizes().begin(),
                              shortcuts_icon_sizes_proto.icon_sizes().end()));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MASKABLE,
        std::vector<SquareSizePx>(
            shortcuts_icon_sizes_proto.icon_sizes_maskable().begin(),
            shortcuts_icon_sizes_proto.icon_sizes_maskable().end()));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MONOCHROME,
        std::vector<SquareSizePx>(
            shortcuts_icon_sizes_proto.icon_sizes_monochrome().begin(),
            shortcuts_icon_sizes_proto.icon_sizes_monochrome().end()));

    shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
  }
  // Due to the bitmaps possibly being not populated (see
  // https://crbug.com/1427444), we just have empty bitmap data in that case.
  while (shortcuts_menu_icons_sizes.size() < shortcut_menu_item_size) {
    shortcuts_menu_icons_sizes.emplace_back();
  }
  if (shortcut_menu_item_size < shortcuts_menu_icons_sizes.size()) {
    RecordProtoParseResult(
        ProtoParseResult::kMoreDownloadedShortcutIconsThanInfos);
    DLOG(ERROR) << "WebApp proto had more downloaded shortcut icons than infos";
    return nullptr;
  }
  CHECK_EQ(shortcuts_menu_item_infos.size(), shortcuts_menu_icons_sizes.size());
  for (size_t i = 0; i < shortcut_menu_item_size; ++i) {
    shortcuts_menu_item_infos[i].downloaded_icon_sizes =
        std::move(shortcuts_menu_icons_sizes[i]);
  }
  // All elements have been moved.
  shortcuts_menu_icons_sizes.clear();
  web_app->SetShortcutsMenuInfo(std::move(shortcuts_menu_item_infos));

  std::vector<std::string> additional_search_terms;
  for (const std::string& additional_search_term :
       proto.additional_search_terms()) {
    if (additional_search_term.empty()) {
      RecordProtoParseResult(ProtoParseResult::kEmptyAdditionalSearchTerm);
      DLOG(ERROR) << "WebApp AdditionalSearchTerms proto action parse error";
      return nullptr;
    }
    additional_search_terms.push_back(additional_search_term);
  }
  web_app->SetAdditionalSearchTerms(std::move(additional_search_terms));

  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;
  for (const auto& protocol_handler_proto : proto.protocol_handlers()) {
    if (!protocol_handler_proto.has_protocol() ||
        !protocol_handler_proto.has_url()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidProtocolHandler);
      DLOG(ERROR) << "WebApp proto Protocol Handler parse error";
      return nullptr;
    }
    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol = protocol_handler_proto.protocol();
    GURL protocol_handler_url(protocol_handler_proto.url());
    if (protocol_handler_url.is_empty() || !protocol_handler_url.is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidProtocolHandlerUrl);
      DLOG(ERROR) << "WebApp ProtocolHandler proto url parse error: "
                  << protocol_handler_url.possibly_invalid_spec();
      return nullptr;
    }
    protocol_handler.url = protocol_handler_url;

    protocol_handlers.push_back(std::move(protocol_handler));
  }
  web_app->SetProtocolHandlers(std::move(protocol_handlers));

  std::vector<std::string> allowed_launch_protocols;
  for (const std::string& allowed_launch_protocol :
       proto.allowed_launch_protocols()) {
    if (allowed_launch_protocol.empty()) {
      RecordProtoParseResult(ProtoParseResult::kEmptyAllowedLaunchProtocol);
      DLOG(ERROR) << "WebApp AllowedLaunchProtocols proto action parse error";
      return nullptr;
    }
    allowed_launch_protocols.push_back(allowed_launch_protocol);
  }
  web_app->SetAllowedLaunchProtocols(std::move(allowed_launch_protocols));

  std::vector<std::string> disallowed_launch_protocols;
  for (const std::string& disallowed_launch_protocol :
       proto.disallowed_launch_protocols()) {
    if (disallowed_launch_protocol.empty()) {
      RecordProtoParseResult(ProtoParseResult::kEmptyDisallowedLaunchProtocol);
      DLOG(ERROR)
          << "WebApp DisallowedLaunchProtocols proto action parse error";
      return nullptr;
    }
    disallowed_launch_protocols.push_back(disallowed_launch_protocol);
  }
  web_app->SetDisallowedLaunchProtocols(std::move(disallowed_launch_protocols));

  base::flat_set<ScopeExtensionInfo> scope_extensions;
  for (const auto& scope_extension_proto : proto.scope_extensions()) {
    if (!scope_extension_proto.has_origin() ||
        !scope_extension_proto.has_has_origin_wildcard()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidScopeExtension);
      DLOG(ERROR) << "WebApp Scope Extension Info proto parse error";
      return nullptr;
    }
    url::Origin origin =
        url::Origin::Create(GURL(scope_extension_proto.origin()));
    if (origin.opaque()) {
      RecordProtoParseResult(ProtoParseResult::kOpaqueScopeExtensionOrigin);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `origin` is opaque: "
                  << scope_extension_proto.origin();
      return nullptr;
    }
    if (origin == url::Origin()) {
      RecordProtoParseResult(ProtoParseResult::kEmptyScopeExtensionOrigin);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `origin` is empty";
      return nullptr;
    }
    if (!GURL(scope_extension_proto.scope()).is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidScopeExtensionScope);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `scope` url is invalid: "
                  << scope_extension_proto.scope();
      return nullptr;
    }

    auto scope_extension =
        ScopeExtensionInfo::CreateForProto(scope_extension_proto);

    scope_extensions.insert(std::move(scope_extension));
  }
  web_app->SetScopeExtensions(std::move(scope_extensions));

  base::flat_set<ScopeExtensionInfo> valid_scope_extensions;
  for (const auto& scope_extension_proto : proto.scope_extensions_validated()) {
    url::Origin origin =
        url::Origin::Create(GURL(scope_extension_proto.origin()));
    if (origin.opaque()) {
      RecordProtoParseResult(ProtoParseResult::kOpaqueValidatedScopeExtension);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `origin` is opaque: "
                  << scope_extension_proto.origin();
      return nullptr;
    }
    if (origin == url::Origin()) {
      RecordProtoParseResult(
          ProtoParseResult::kEmptyValidatedScopeExtensionOrigin);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `origin` is empty";
      return nullptr;
    }
    if (!GURL(scope_extension_proto.scope()).is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidScopeExtensionValidated);
      DLOG(ERROR) << "WebAppScopeExtensionProto's `scope` url is invalid: "
                  << scope_extension_proto.scope();
      return nullptr;
    }

    auto scope_extension =
        ScopeExtensionInfo::CreateForProto(scope_extension_proto);

    if (!scope_extension.origin.IsSameOriginWith(scope_extension.scope)) {
      RecordProtoParseResult(
          ProtoParseResult::kScopeExtensionOriginMismatchWithScope);
      return nullptr;
    }

    valid_scope_extensions.insert(std::move(scope_extension));
  }
  web_app->SetValidatedScopeExtensions(std::move(valid_scope_extensions));

  if (proto.has_lock_screen_start_url()) {
    web_app->SetLockScreenStartUrl(GURL(proto.lock_screen_start_url()));
  }

  if (proto.has_note_taking_new_note_url()) {
    web_app->SetNoteTakingNewNoteUrl(GURL(proto.note_taking_new_note_url()));
  }

  if (proto.has_user_run_on_os_login_mode()) {
    web_app->SetRunOnOsLoginMode(
        ToRunOnOsLoginMode(proto.user_run_on_os_login_mode()));
  }

  if (proto.has_manifest_url()) {
    GURL manifest_url(proto.manifest_url());
    if (manifest_url.is_empty() || !manifest_url.is_valid()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidManifestUrl);
      DLOG(ERROR) << "WebApp proto manifest_url parse error: "
                  << manifest_url.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetManifestUrl(manifest_url);
  }

  if (proto.has_file_handler_approval_state()) {
    web_app->SetFileHandlerApprovalState(
        ProtoToApiApprovalState(proto.file_handler_approval_state()));
  }

  if (proto.has_window_controls_overlay_enabled()) {
    web_app->SetWindowControlsOverlayEnabled(
        proto.window_controls_overlay_enabled());
  }

  if (proto.has_launch_handler()) {
    web_app->SetLaunchHandler(ProtoToLaunchHandler(proto.launch_handler()));
  }

  if (proto.has_parent_app_id()) {
    web_app->parent_app_id_ = proto.parent_app_id();
  }

  if (proto.permissions_policy_size()) {
    network::ParsedPermissionsPolicy policy;
    const auto& name_to_feature_map =
        blink::GetPermissionsPolicyNameToFeatureMap();
    for (const auto& decl_proto : proto.permissions_policy()) {
      network::ParsedPermissionsPolicyDeclaration decl;
      const auto feature_enum = name_to_feature_map.find(decl_proto.feature());
      if (feature_enum == name_to_feature_map.end()) {
        continue;
      }
      decl.feature = feature_enum->second;

      for (const std::string& origin : decl_proto.allowed_origins()) {
        std::optional<network::OriginWithPossibleWildcards>
            maybe_origin_with_possible_wildcards =
                network::OriginWithPossibleWildcards::Parse(
                    origin,
                    network::OriginWithPossibleWildcards::NodeType::kHeader);
        if (maybe_origin_with_possible_wildcards.has_value()) {
          decl.allowed_origins.emplace_back(
              *maybe_origin_with_possible_wildcards);
        }
      }
      decl.matches_all_origins = decl_proto.matches_all_origins();
      decl.matches_opaque_src = decl_proto.matches_opaque_src();
      policy.push_back(decl);
    }
    web_app->SetPermissionsPolicy(policy);
  }

  WebApp::ExternalConfigMap management_to_external_config;
  for (const auto& management_proto :
       proto.management_to_external_config_info()) {
    WebApp::ExternalManagementConfig config;
    base::flat_set<GURL> install_urls;
    for (const auto& install_url_proto : management_proto.install_urls()) {
      GURL install_url(install_url_proto);
      if (install_url.is_empty() || !install_url.is_valid()) {
        RecordProtoParseResult(ProtoParseResult::kInvalidInstallUrl);
        DLOG(ERROR) << "WebApp proto install_url parse error: "
                    << install_url.possibly_invalid_spec();
        return nullptr;
      }
      install_urls.emplace(install_url);
    }
    base::flat_set<std::string> additional_policy_ids;
    for (const auto& policy_id : management_proto.additional_policy_ids()) {
      if (policy_id.empty()) {
        RecordProtoParseResult(ProtoParseResult::kEmptyPolicyId);
        DLOG(ERROR) << "WebApp proto empty policy_id";
        return nullptr;
      }
      additional_policy_ids.emplace(policy_id);
    }

    config.is_placeholder = management_proto.is_placeholder();
    config.install_urls = std::move(install_urls);
    config.additional_policy_ids = std::move(additional_policy_ids);
    management_to_external_config.insert_or_assign(
        ProtoToWebAppManagement(management_proto.management()),
        std::move(config));
  }
  web_app->SetWebAppManagementExternalConfigMap(management_to_external_config);

  if (proto.has_tab_strip()) {
    web_app->SetTabStrip(ProtoToTabStrip(proto.tab_strip()));
  }

  if (proto.has_current_os_integration_states()) {
    web_app->SetCurrentOsIntegrationStates(
        proto.current_os_integration_states());
  }

  if (proto.has_app_size_in_bytes()) {
    web_app->SetAppSizeInBytes(proto.app_size_in_bytes());
  }

  if (proto.has_data_size_in_bytes()) {
    web_app->SetDataSizeInBytes(proto.data_size_in_bytes());
  }

  if (proto.has_always_show_toolbar_in_fullscreen()) {
    web_app->SetAlwaysShowToolbarInFullscreen(
        proto.always_show_toolbar_in_fullscreen());
  }

  if (proto.has_isolation_data()) {
    auto iwa_version = IwaVersion::Create(proto.isolation_data().version());

    if (!iwa_version.has_value()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidIsolationDataVersion);
      DLOG(ERROR) << "WebApp proto isolation_data.version parse error: cannot "
                     "deserialize version: "
                  << IwaVersion::GetErrorString(iwa_version.error());
      return nullptr;
    }

    base::expected<IsolatedWebAppStorageLocation, std::string> location =
        ProtoToIsolationDataLocation(proto.isolation_data());
    if (!location.has_value()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidIsolationDataLocation);
      DLOG(ERROR) << "WebApp proto isolation_data.location" << location.error();
      return nullptr;
    }

    auto isolation_data_builder =
        IsolationData::Builder(*std::move(location), *std::move(iwa_version));

    const google::protobuf::RepeatedPtrField<std::string>& partitions =
        proto.isolation_data().controlled_frame_partitions();
    isolation_data_builder.SetControlledFramePartitions(
        {partitions.begin(), partitions.end()});

    if (proto.isolation_data().has_pending_update_info()) {
      const auto& pending_update_info_proto =
          proto.isolation_data().pending_update_info();

      base::expected<IsolatedWebAppStorageLocation, std::string>
          pending_location =
              ProtoToIsolationDataLocation(pending_update_info_proto);
      if (!pending_location.has_value()) {
        RecordProtoParseResult(
            ProtoParseResult::kInvalidPendingUpdateInfoLocation);
        DLOG(ERROR)
            << "WebApp proto isolation_data.pending_update_info.location"
            << pending_location.error();
        return nullptr;
      }
      if (pending_location->dev_mode() != location->dev_mode()) {
        RecordProtoParseResult(
            ProtoParseResult::kDevModeMismatchInIsolationData);
        DLOG(ERROR) << "WebApp proto isolation_data.pending_update_info "
                       "deserialization error: "
                       "isolation_data.pending_update_info.location and "
                       "isolation_data.location must both be in dev mode or "
                       "not in dev mode.";
        return nullptr;
      }

      auto pending_iwa_version =
          IwaVersion::Create(pending_update_info_proto.version());

      if (!pending_iwa_version.has_value()) {
        RecordProtoParseResult(
            ProtoParseResult::kInvalidPendingUpdateInfoVersion);
        DLOG(ERROR)
            << "WebApp proto isolation_data.pending_update_info.version parse "
               "error: cannot deserialize version: "
            << IwaVersion::GetErrorString(pending_iwa_version.error());
        return nullptr;
      }

      std::optional<IsolatedWebAppIntegrityBlockData>
          pending_integrity_block_data;
      if (pending_update_info_proto.has_integrity_block_data()) {
        auto result = IsolatedWebAppIntegrityBlockData::FromProto(
            pending_update_info_proto.integrity_block_data());
        if (!result.has_value()) {
          RecordProtoParseResult(
              ProtoParseResult::kInvalidPendingUpdateIntegrityBlockData);
          DLOG(ERROR) << "WebApp proto "
                         "isolation_data.pending_update_info.integrity_block "
                         "data parse error: "
                      << result.error();
          return nullptr;
        }
        pending_integrity_block_data = std::move(result.value());
      }

      isolation_data_builder.SetPendingUpdateInfo(
          IsolationData::PendingUpdateInfo(
              *std::move(pending_location), *std::move(pending_iwa_version),
              std::move(pending_integrity_block_data)));
    }

    if (proto.isolation_data().has_integrity_block_data()) {
      auto result = IsolatedWebAppIntegrityBlockData::FromProto(
          proto.isolation_data().integrity_block_data());
      if (!result.has_value()) {
        RecordProtoParseResult(ProtoParseResult::kInvalidIntegrityBlockData);
        DLOG(ERROR)
            << "WebApp proto isolation_data.integrity_block_data parse error: "
            << result.error();
        return nullptr;
      }
      isolation_data_builder.SetIntegrityBlockData(std::move(*result));
    }

    if (proto.isolation_data().has_update_manifest_url()) {
      GURL update_manifest_url(proto.isolation_data().update_manifest_url());
      if (!update_manifest_url.is_valid()) {
        RecordProtoParseResult(ProtoParseResult::kInvalidUpdateManifestUrlIwa);
        DLOG(ERROR) << "WebApp proto isolation_data.update_manifest_url is not "
                       "a valid GURL.";
        return nullptr;
      }
      isolation_data_builder.SetUpdateManifestUrl(
          std::move(update_manifest_url));
    }

    if (proto.isolation_data().has_update_channel()) {
      auto update_channel =
          UpdateChannel::Create(proto.isolation_data().update_channel());
      if (!update_channel.has_value()) {
        RecordProtoParseResult(ProtoParseResult::kInvalidUpdateChannel);
        DLOG(ERROR)
            << "WebApp proto isolation_data.update_channel is not valid.";
        return nullptr;
      }
      isolation_data_builder.SetUpdateChannel(std::move(*update_channel));
    }
    if (proto.isolation_data().has_opened_tabs_counter_notification_state()) {
      isolation_data_builder.SetOpenedTabsCounterNotificationState(
          IsolationData::OpenedTabsCounterNotificationState(
              proto.isolation_data().opened_tabs_counter_notification_state()));
    }
    web_app->SetIsolationData(std::move(isolation_data_builder).Build());
  }
  if (proto.has_user_link_capturing_preference()) {
    web_app->SetLinkCapturingUserPreference(
        proto.user_link_capturing_preference());
  }

  if (proto.has_latest_install_time()) {
    web_app->SetLatestInstallTime(
        syncer::ProtoTimeToTime(proto.latest_install_time()));
  } else if (proto.has_first_install_time()) {
    web_app->SetLatestInstallTime(
        syncer::ProtoTimeToTime(proto.first_install_time()));
  }

  if (proto.has_generated_icon_fix()) {
    if (!generated_icon_fix_util::IsValid(proto.generated_icon_fix())) {
      RecordProtoParseResult(ProtoParseResult::kInvalidGeneratedIconFix);
      return nullptr;
    }
    web_app->SetGeneratedIconFix(proto.generated_icon_fix());
  }

  if (proto.has_supported_links_offer_ignore_count()) {
    web_app->SetSupportedLinksOfferIgnoreCount(
        proto.supported_links_offer_ignore_count());
  }

  if (proto.has_supported_links_offer_dismiss_count()) {
    web_app->SetSupportedLinksOfferDismissCount(
        proto.supported_links_offer_dismiss_count());
  }

  web_app->SetIsDiyApp(proto.is_diy_app());

  web_app->SetWasShortcutApp(proto.was_shortcut_app());

  web_app->SetDiyAppIconsMaskedOnMac(proto.diy_app_icons_masked_on_mac());

  std::vector<blink::Manifest::RelatedApplication> related_applications;
  for (const auto& related_application_proto : proto.related_applications()) {
    blink::Manifest::RelatedApplication related_application;
    if (related_application_proto.has_platform()) {
      related_application.platform = std::make_optional(
          base::UTF8ToUTF16(related_application_proto.platform()));
    }
    related_application.url = GURL(related_application_proto.url());
    if (related_application_proto.has_id()) {
      related_application.id =
          std::make_optional(base::UTF8ToUTF16(related_application_proto.id()));
    }
    related_applications.push_back(std::move(related_application));
  }
  web_app->SetRelatedApplications(std::move(related_applications));

  if (proto.has_pending_update_info()) {
    // Exit early if there is a `PendingUpdateInfo` that is completely empty.
    if (!proto.pending_update_info().has_name() &&
        proto.pending_update_info().trusted_icons().empty() &&
        proto.pending_update_info().manifest_icons().empty() &&
        proto.pending_update_info().downloaded_trusted_icons().empty() &&
        proto.pending_update_info().downloaded_manifest_icons().empty() &&
        !proto.pending_update_info().has_was_ignored()) {
      RecordProtoParseResult(ProtoParseResult::kEmptyPendingUpdateInfo);
      return nullptr;
    }

    // Exit early if trusted icons is populated but manifest icons is not, and
    // vice versa.
    if (proto.pending_update_info().trusted_icons().empty() !=
        proto.pending_update_info().manifest_icons().empty()) {
      RecordProtoParseResult(
          ProtoParseResult::kMismatchedPendingUpdateInfoIcons);
      return nullptr;
    }

    // Populate manifest_icons and trusted_icons only if both are populated.
    if (!proto.pending_update_info().manifest_icons().empty() &&
        !proto.pending_update_info().trusted_icons().empty()) {
      for (const auto& icon : proto.pending_update_info().manifest_icons()) {
        if (!icon.has_url() || !icon.has_purpose()) {
          RecordProtoParseResult(
              ProtoParseResult::kInvalidPendingUpdateManifestIcons);
          return nullptr;
        }
      }
      for (const auto& icon : proto.pending_update_info().trusted_icons()) {
        if (!icon.has_url() || !icon.has_purpose()) {
          RecordProtoParseResult(
              ProtoParseResult::kInvalidPendingUpdateTrustedIcons);
          return nullptr;
        }
      }
      // If manifest_icons and trusted_icons are populated, then
      // downloaded_trusted_icon_sizes and downloaded_manifest_icon_sizes must
      // also be populated.
      if (proto.pending_update_info().downloaded_trusted_icons().empty() ||
          proto.pending_update_info().downloaded_manifest_icons().empty()) {
        RecordProtoParseResult(
            ProtoParseResult::kMissingDownloadedIconsForPendingUpdate);
        return nullptr;
      }

      for (const auto& icon :
           proto.pending_update_info().downloaded_manifest_icons()) {
        // It's fine if there are no sizes specified for a purpose, but the
        // purpose has to exist.
        if (!icon.has_purpose()) {
          RecordProtoParseResult(
              ProtoParseResult::kInvalidDownloadedManifestIconForPendingUpdate);
          return nullptr;
        }
      }
      for (const auto& icon :
           proto.pending_update_info().downloaded_trusted_icons()) {
        // It's fine if there are no sizes specified for a purpose, but the
        // purpose has to exist.
        if (!icon.has_purpose()) {
          RecordProtoParseResult(
              ProtoParseResult::kInvalidDownloadedTrustedIconForPendingUpdate);
          return nullptr;
        }
      }
    }

    // The `was_ignored` field should always be set, and default initialized by
    // database migration in case of proto version differences. This not being
    // set is an error case.
    if (!proto.pending_update_info().has_was_ignored()) {
      RecordProtoParseResult(
          ProtoParseResult::kMissingWasIgnoredForPendingUpdate);
      return nullptr;
    }

    web_app->SetPendingUpdateInfo(proto.pending_update_info());
  }

  std::optional<std::vector<apps::IconInfo>> parsed_trusted_icons =
      ParseAppIconInfos("WebApp", proto.trusted_icons());
  if (!parsed_trusted_icons) {
    RecordProtoParseResult(ProtoParseResult::kInvalidParsedTrustedIcons);
    // ParseWebAppIconInfos() reports any errors.
    return nullptr;
  }
  web_app->SetTrustedIcons(std::move(parsed_trusted_icons.value()));

  std::vector<SquareSizePx> trusted_icon_sizes_any;
  for (int32_t size : proto.stored_trusted_icon_sizes_any()) {
    trusted_icon_sizes_any.push_back(size);
  }
  web_app->SetStoredTrustedIconSizes(
      IconPurpose::ANY, SortedSizesPx(std::move(trusted_icon_sizes_any)));

  std::vector<SquareSizePx> trusted_icon_sizes_maskable;
  for (int32_t size : proto.stored_trusted_icon_sizes_maskable()) {
    trusted_icon_sizes_maskable.push_back(size);
  }
  web_app->SetStoredTrustedIconSizes(
      IconPurpose::MASKABLE,
      SortedSizesPx(std::move(trusted_icon_sizes_maskable)));

  auto borderless_url_patterns = ToUrlPatterns(proto.borderless_url_patterns());
  if (!borderless_url_patterns.has_value()) {
    RecordProtoParseResult(ProtoParseResult::kInvalidBorderlessUrlPatterns);
    return nullptr;
  }
  web_app->SetBorderlessUrlPatterns(std::move(borderless_url_patterns.value()));

  std::deque<AppInstalledBy> installed_by_data;
  for (const auto& installed_by_proto : proto.installed_by()) {
    std::optional<AppInstalledBy> installed_by =
        AppInstalledBy::Parse(installed_by_proto);
    if (!installed_by.has_value()) {
      RecordProtoParseResult(ProtoParseResult::kInvalidInstalledBy);
      DLOG(ERROR) << "WebApp proto Installed By field parse error";
      return nullptr;
    }
    installed_by_data.push_back(std::move(installed_by.value()));
  }
  web_app->SetInstalledBy(InstalledByPassKey(), std::move(installed_by_data));

  RecordProtoParseResult(ProtoParseResult::kSuccess);
  return web_app;
}

std::unique_ptr<proto::WebApp> WebAppToProto(const WebApp& web_app) {
  auto local_data = std::make_unique<proto::WebApp>();

  // Required fields:
  const GURL start_url = web_app.start_url();
  DCHECK(start_url.is_valid());

  DCHECK(!web_app.app_id().empty());
  DCHECK(web_app.manifest_id().is_valid());

  // Set sync data to sync proto.
  *(local_data->mutable_sync_data()) = web_app.sync_proto();

  local_data->set_name(web_app.untranslated_name());

  DCHECK(!web_app.sources_.empty() || web_app.is_uninstalling());
  local_data->mutable_sources()->set_system(
      web_app.sources_.Has(WebAppManagement::kSystem));
  local_data->mutable_sources()->set_policy(
      web_app.sources_.Has(WebAppManagement::kPolicy));
  local_data->mutable_sources()->set_web_app_store(
      web_app.sources_.Has(WebAppManagement::kWebAppStore));
  local_data->mutable_sources()->set_sync(
      web_app.sources_.Has(WebAppManagement::kSync));
  local_data->mutable_sources()->set_user_installed(
      web_app.sources_.Has(WebAppManagement::kUserInstalled));
  local_data->mutable_sources()->set_default_(
      web_app.sources_.Has(WebAppManagement::kDefault));
  local_data->mutable_sources()->set_sub_app(
      web_app.sources_.Has(WebAppManagement::kSubApp));
  local_data->mutable_sources()->set_kiosk(
      web_app.sources_.Has(WebAppManagement::kKiosk));
  local_data->mutable_sources()->set_iwa_shimless_rma(
      web_app.sources_.Has(WebAppManagement::kIwaShimlessRma));
  local_data->mutable_sources()->set_iwa_policy(
      web_app.sources_.Has(WebAppManagement::kIwaPolicy));
  local_data->mutable_sources()->set_iwa_user_installed(
      web_app.sources_.Has(WebAppManagement::kIwaUserInstalled));
  local_data->mutable_sources()->set_oem(
      web_app.sources_.Has(WebAppManagement::kOem));
  local_data->mutable_sources()->set_one_drive_integration(
      web_app.sources_.Has(WebAppManagement::kOneDriveIntegration));
  local_data->mutable_sources()->set_aps_default(
      web_app.sources_.Has(WebAppManagement::kApsDefault));

  local_data->set_install_state(web_app.install_state());

  // Optional fields:
  if (web_app.launch_query_params()) {
    local_data->set_launch_query_params(*web_app.launch_query_params());
  }

  if (web_app.display_mode() != DisplayMode::kUndefined) {
    local_data->set_display_mode(
        ToWebAppProtoDisplayMode(web_app.display_mode()));
  }

  for (const DisplayMode& display_mode : web_app.display_mode_override()) {
    local_data->add_display_mode_override(
        ToWebAppProtoDisplayMode(display_mode));
  }

  local_data->set_description(web_app.untranslated_description());
  if (!web_app.scope().is_empty()) {
    local_data->set_scope(web_app.scope().spec());
  }
  if (web_app.theme_color().has_value()) {
    local_data->set_theme_color(web_app.theme_color().value());
  }
  if (web_app.dark_mode_theme_color().has_value()) {
    local_data->set_dark_mode_theme_color(
        web_app.dark_mode_theme_color().value());
  }
  if (web_app.background_color().has_value()) {
    local_data->set_background_color(web_app.background_color().value());
  }
  if (web_app.dark_mode_background_color().has_value()) {
    local_data->set_dark_mode_background_color(
        web_app.dark_mode_background_color().value());
  }
  if (!web_app.last_badging_time().is_null()) {
    local_data->set_last_badging_time(
        syncer::TimeToProtoTime(web_app.last_badging_time()));
  }
  if (!web_app.last_launch_time().is_null()) {
    local_data->set_last_launch_time(
        syncer::TimeToProtoTime(web_app.last_launch_time()));
  }
  if (!web_app.first_install_time().is_null()) {
    local_data->set_first_install_time(
        syncer::TimeToProtoTime(web_app.first_install_time()));
  }
  if (!web_app.manifest_update_time().is_null()) {
    local_data->set_manifest_update_time(
        syncer::TimeToProtoTime(web_app.manifest_update_time()));
  }

  if (web_app.latest_install_source()) {
    local_data->set_latest_install_source(
        static_cast<int>(*web_app.latest_install_source()));
  }

  if (web_app.chromeos_data().has_value()) {
    auto& chromeos_data = web_app.chromeos_data().value();
    auto* mutable_chromeos_data = local_data->mutable_chromeos_data();
    mutable_chromeos_data->set_show_in_launcher(chromeos_data.show_in_launcher);
    mutable_chromeos_data->set_show_in_search_and_shelf(
        chromeos_data.show_in_search_and_shelf);
    mutable_chromeos_data->set_show_in_management(
        chromeos_data.show_in_management);
    mutable_chromeos_data->set_is_disabled(chromeos_data.is_disabled);
    mutable_chromeos_data->set_oem_installed(chromeos_data.oem_installed);
    mutable_chromeos_data->set_handles_file_open_intents(
        chromeos_data.handles_file_open_intents);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (web_app.client_data().system_web_app_data.has_value()) {
    auto& swa_data = web_app.client_data().system_web_app_data.value();

    auto* mutable_swa_data =
        local_data->mutable_client_data()->mutable_system_web_app_data();
    mutable_swa_data->set_system_app_type(
        static_cast<ash::SystemWebAppDataProto_SystemWebAppType>(
            swa_data.system_app_type));
  }
#endif

  local_data->set_user_run_on_os_login_mode(
      ToWebAppProtoRunOnOsLoginMode(web_app.run_on_os_login_mode()));
  local_data->set_is_from_sync_and_pending_installation(
      web_app.is_from_sync_and_pending_installation());
  local_data->set_is_uninstalling(web_app.is_uninstalling());

  for (const apps::IconInfo& icon_info : web_app.manifest_icons()) {
    *(local_data->add_manifest_icons()) = AppIconInfoToSyncProto(icon_info);
  }

  for (SquareSizePx size : web_app.downloaded_icon_sizes(IconPurpose::ANY)) {
    local_data->add_downloaded_icon_sizes_purpose_any(size);
  }
  for (SquareSizePx size :
       web_app.downloaded_icon_sizes(IconPurpose::MASKABLE)) {
    local_data->add_downloaded_icon_sizes_purpose_maskable(size);
  }
  for (SquareSizePx size :
       web_app.downloaded_icon_sizes(IconPurpose::MONOCHROME)) {
    local_data->add_downloaded_icon_sizes_purpose_monochrome(size);
  }

  local_data->set_is_generated_icon(web_app.is_generated_icon());

  for (const auto& file_handler : web_app.file_handlers()) {
    proto::WebAppFileHandler* file_handler_proto =
        local_data->add_file_handlers();
    DCHECK(file_handler.action.is_valid());
    file_handler_proto->set_action(file_handler.action.spec());
    file_handler_proto->set_display_name(
        base::UTF16ToUTF8(file_handler.display_name));
    file_handler_proto->set_launch_type(
        LaunchTypeToProto(file_handler.launch_type));

    for (const auto& accept_entry : file_handler.accept) {
      proto::WebAppFileHandlerAccept* accept_entry_proto =
          file_handler_proto->add_accept();
      accept_entry_proto->set_mimetype(accept_entry.mime_type);

      for (const auto& file_extension : accept_entry.file_extensions) {
        accept_entry_proto->add_file_extensions(file_extension);
      }
    }

    for (const apps::IconInfo& icon_info : file_handler.downloaded_icons) {
      *(file_handler_proto->add_downloaded_icons()) =
          AppIconInfoToSyncProto(icon_info);
    }
  }

  if (web_app.share_target()) {
    const apps::ShareTarget& share_target = *web_app.share_target();
    auto* const mutable_share_target = local_data->mutable_share_target();
    mutable_share_target->set_action(share_target.action.spec());
    mutable_share_target->set_method(MethodToProto(share_target.method));
    mutable_share_target->set_enctype(EnctypeToProto(share_target.enctype));

    const apps::ShareTarget::Params& params = share_target.params;
    auto* const mutable_share_target_params =
        mutable_share_target->mutable_params();
    if (!params.title.empty()) {
      mutable_share_target_params->set_title(params.title);
    }
    if (!params.text.empty()) {
      mutable_share_target_params->set_text(params.text);
    }
    if (!params.url.empty()) {
      mutable_share_target_params->set_url(params.url);
    }

    for (const auto& files_entry : params.files) {
      proto::ShareTargetParamsFile* mutable_share_target_files =
          mutable_share_target_params->add_files();
      mutable_share_target_files->set_name(files_entry.name);

      for (const auto& file_type : files_entry.accept) {
        mutable_share_target_files->add_accept(file_type);
      }
    }
  }

  for (const WebAppShortcutsMenuItemInfo& shortcut_info :
       web_app.shortcuts_menu_item_infos()) {
    proto::WebAppShortcutsMenuItemInfo* shortcut_info_proto =
        local_data->add_shortcuts_menu_item_infos();
    shortcut_info_proto->set_name(base::UTF16ToUTF8(shortcut_info.name));
    shortcut_info_proto->set_url(shortcut_info.url.spec());
    for (IconPurpose purpose : kIconPurposes) {
      for (const WebAppShortcutsMenuItemInfo::Icon& icon_info :
           shortcut_info.GetShortcutIconInfosForPurpose(purpose)) {
        sync_pb::WebAppIconInfo* shortcut_icon_info_proto;
        switch (purpose) {
          case IconPurpose::ANY:
            shortcut_icon_info_proto =
                shortcut_info_proto->add_shortcut_manifest_icons();
            break;
          case IconPurpose::MASKABLE:
            shortcut_icon_info_proto =
                shortcut_info_proto->add_shortcut_manifest_icons_maskable();
            break;
          case IconPurpose::MONOCHROME:
            shortcut_icon_info_proto =
                shortcut_info_proto->add_shortcut_manifest_icons_monochrome();
            break;
        }

        DCHECK(!icon_info.url.is_empty());
        shortcut_icon_info_proto->set_url(icon_info.url.spec());
        shortcut_icon_info_proto->set_size_in_px(icon_info.square_size_px);
      }
    }

    const IconSizes& icon_sizes = shortcut_info.downloaded_icon_sizes;
    proto::DownloadedShortcutsMenuIconSizes* icon_sizes_proto =
        local_data->add_downloaded_shortcuts_menu_icons_sizes();
    for (const SquareSizePx& icon_size :
         icon_sizes.GetSizesForPurpose(IconPurpose::ANY)) {
      icon_sizes_proto->add_icon_sizes(icon_size);
    }
    for (const SquareSizePx& icon_size :
         icon_sizes.GetSizesForPurpose(IconPurpose::MASKABLE)) {
      icon_sizes_proto->add_icon_sizes_maskable(icon_size);
    }
    for (const SquareSizePx& icon_size :
         icon_sizes.GetSizesForPurpose(IconPurpose::MONOCHROME)) {
      icon_sizes_proto->add_icon_sizes_monochrome(icon_size);
    }
  }

  for (const auto& additional_search_term : web_app.additional_search_terms()) {
    // Additional search terms should be sanitized before being added here.
    DCHECK(!additional_search_term.empty());
    local_data->add_additional_search_terms(additional_search_term);
  }

  for (const auto& protocol_handler : web_app.protocol_handlers()) {
    proto::WebAppProtocolHandler* protocol_handler_proto =
        local_data->add_protocol_handlers();
    protocol_handler_proto->set_protocol(protocol_handler.protocol);
    protocol_handler_proto->set_url(protocol_handler.url.spec());
  }

  for (const auto& allowed_launch_protocols :
       web_app.allowed_launch_protocols()) {
    DCHECK(!allowed_launch_protocols.empty());
    local_data->add_allowed_launch_protocols(allowed_launch_protocols);
  }

  for (const auto& disallowed_launch_protocols :
       web_app.disallowed_launch_protocols()) {
    DCHECK(!disallowed_launch_protocols.empty());
    local_data->add_disallowed_launch_protocols(disallowed_launch_protocols);
  }

  for (const auto& scope_extension : web_app.scope_extensions()) {
    proto::WebAppScopeExtension* scope_extension_proto =
        local_data->add_scope_extensions();
    scope_extension_proto->set_origin(scope_extension.origin.Serialize());
    scope_extension_proto->set_scope(scope_extension.scope.spec());
    scope_extension_proto->set_has_origin_wildcard(
        scope_extension.has_origin_wildcard);
  }

  for (const auto& valid_extension : web_app.validated_scope_extensions()) {
    proto::WebAppScopeExtension* scope_extension_proto =
        local_data->add_scope_extensions_validated();
    scope_extension_proto->set_origin(valid_extension.origin.Serialize());
    CHECK(valid_extension.scope.is_valid());
    scope_extension_proto->set_scope(valid_extension.scope.spec());
    scope_extension_proto->set_has_origin_wildcard(
        valid_extension.has_origin_wildcard);
  }

  if (web_app.lock_screen_start_url().is_valid()) {
    local_data->set_lock_screen_start_url(
        web_app.lock_screen_start_url().spec());
  }

  if (web_app.note_taking_new_note_url().is_valid()) {
    local_data->set_note_taking_new_note_url(
        web_app.note_taking_new_note_url().spec());
  }

  if (web_app.manifest_url().is_valid()) {
    local_data->set_manifest_url(web_app.manifest_url().spec());
  }

  local_data->set_file_handler_approval_state(
      ApiApprovalStateToProto(web_app.file_handler_approval_state()));

  local_data->set_window_controls_overlay_enabled(
      web_app.window_controls_overlay_enabled());

  if (web_app.launch_handler()) {
    *local_data->mutable_launch_handler() =
        LaunchHandlerToProto(*web_app.launch_handler());
  }

  if (web_app.parent_app_id_) {
    local_data->set_parent_app_id(*web_app.parent_app_id_);
  }

  if (!web_app.permissions_policy().empty()) {
    auto& policy = *local_data->mutable_permissions_policy();
    const auto& feature_to_name_map =
        blink::GetPermissionsPolicyFeatureToNameMap();
    for (const auto& decl : web_app.permissions_policy()) {
      proto::WebAppPermissionsPolicy proto_policy;
      const auto feature_name = feature_to_name_map.find(decl.feature);
      if (feature_name == feature_to_name_map.end()) {
        continue;
      }
      const std::string feature_string(feature_name->second);
      proto_policy.set_feature(feature_string);
      for (const auto& allowed_origin : GetSerializedAllowedOrigins(decl)) {
        proto_policy.add_allowed_origins(allowed_origin);
      }
      proto_policy.set_matches_all_origins(decl.matches_all_origins);
      proto_policy.set_matches_opaque_src(decl.matches_opaque_src);
      policy.Add(std::move(proto_policy));
    }
  }

  if (!web_app.management_to_external_config_map().empty()) {
    for (const auto& [source, external_config] :
         web_app.management_to_external_config_map()) {
      proto::ManagementToExternalConfigInfo* management_config_proto =
          local_data->add_management_to_external_config_info();
      management_config_proto->set_management(WebAppManagementToProto(source));
      management_config_proto->set_is_placeholder(
          external_config.is_placeholder);
      for (const auto& url : external_config.install_urls) {
        DCHECK(url.is_valid());
        management_config_proto->add_install_urls(url.spec());
      }
      for (const auto& policy_id : external_config.additional_policy_ids) {
        DCHECK(!policy_id.empty());
        management_config_proto->add_additional_policy_ids(policy_id);
      }
    }
  }

  if (web_app.tab_strip()) {
    TabStrip tab_strip = web_app.tab_strip().value();

    auto* mutable_tab_strip = local_data->mutable_tab_strip();
    if (std::holds_alternative<TabStrip::Visibility>(tab_strip.home_tab)) {
      mutable_tab_strip->set_home_tab_visibility(TabStripVisibilityToProto(
          std::get<TabStrip::Visibility>(tab_strip.home_tab)));
    } else {
      auto* mutable_home_tab_params =
          mutable_tab_strip->mutable_home_tab_params();

      const std::optional<std::vector<blink::Manifest::ImageResource>>& icons =
          std::get<blink::Manifest::HomeTabParams>(tab_strip.home_tab).icons;
      for (const auto& image_resource : *icons) {
        *(mutable_home_tab_params->add_icons()) =
            AppImageResourceToProto(image_resource);
      }

      const std::vector<blink::SafeUrlPattern>& scope_patterns =
          std::get<blink::Manifest::HomeTabParams>(tab_strip.home_tab)
              .scope_patterns;
      for (const auto& pattern : scope_patterns) {
        *(mutable_home_tab_params->add_scope_patterns()) =
            ToUrlPatternProto(pattern);
      }
    }

    auto* mutable_new_tab_button_params =
        mutable_tab_strip->mutable_new_tab_button_params();
    std::optional<GURL> url = tab_strip.new_tab_button.url;
    if (url) {
      mutable_new_tab_button_params->set_url(url.value().spec());
    }
  }

  *local_data->mutable_current_os_integration_states() =
      web_app.current_os_integration_states();

  if (web_app.app_size_in_bytes().has_value()) {
    local_data->set_app_size_in_bytes(web_app.app_size_in_bytes().value());
  }

  if (web_app.data_size_in_bytes().has_value()) {
    local_data->set_data_size_in_bytes(web_app.data_size_in_bytes().value());
  }

  local_data->set_always_show_toolbar_in_fullscreen(
      web_app.always_show_toolbar_in_fullscreen());

  if (web_app.isolation_data().has_value()) {
    const auto& isolation_data = *web_app.isolation_data();
    auto* mutable_data = local_data->mutable_isolation_data();

    IsolationDataLocationToProto(isolation_data.location(), mutable_data);
    mutable_data->set_version(isolation_data.version().GetString());
    for (const std::string& partition :
         isolation_data.controlled_frame_partitions()) {
      mutable_data->add_controlled_frame_partitions(partition);
    }

    if (isolation_data.pending_update_info().has_value()) {
      const IsolationData::PendingUpdateInfo& pending_update_info =
          *isolation_data.pending_update_info();
      auto* mutable_pending_update_info =
          mutable_data->mutable_pending_update_info();

      CHECK_EQ(isolation_data.location().dev_mode(),
               pending_update_info.location.dev_mode(),
               base::NotFatalUntil::M138)
          << "IsolationData dev_mode mismatch between current location and "
             "pending update location during serialization.";

      IsolationDataLocationToProto(pending_update_info.location,
                                   mutable_pending_update_info);
      mutable_pending_update_info->set_version(
          pending_update_info.version.GetString());
      if (pending_update_info.integrity_block_data) {
        *mutable_pending_update_info->mutable_integrity_block_data() =
            pending_update_info.integrity_block_data->ToProto();
      }
    }

    if (const auto& notification_state =
            isolation_data.opened_tabs_counter_notification_state()) {
      *mutable_data->mutable_opened_tabs_counter_notification_state() =
          notification_state->GetState();
    }

    if (isolation_data.integrity_block_data()) {
      *mutable_data->mutable_integrity_block_data() =
          isolation_data.integrity_block_data()->ToProto();
    }

    if (const auto& update_manifest_url = isolation_data.update_manifest_url();
        update_manifest_url.has_value() && update_manifest_url->is_valid()) {
      mutable_data->set_update_manifest_url(update_manifest_url->spec());
    }

    if (const auto& update_channel = isolation_data.update_channel()) {
      mutable_data->set_update_channel(update_channel->ToString());
    }
  }

  local_data->set_user_link_capturing_preference(
      web_app.user_link_capturing_preference());

  if (!web_app.latest_install_time().is_null()) {
    local_data->set_latest_install_time(
        syncer::TimeToProtoTime(web_app.latest_install_time()));
  }

  if (web_app.generated_icon_fix().has_value()) {
    *local_data->mutable_generated_icon_fix() =
        web_app.generated_icon_fix().value();
  }

  local_data->set_supported_links_offer_ignore_count(
      web_app.supported_links_offer_ignore_count());
  local_data->set_supported_links_offer_dismiss_count(
      web_app.supported_links_offer_dismiss_count());

  local_data->set_is_diy_app(web_app.is_diy_app());

  local_data->set_was_shortcut_app(web_app.was_shortcut_app());

  local_data->set_diy_app_icons_masked_on_mac(
      web_app.diy_app_icons_masked_on_mac());

  for (const auto& related_application : web_app.related_applications()) {
    proto::RelatedApplications* related_application_proto =
        local_data->add_related_applications();
    if (related_application.platform) {
      related_application_proto->set_platform(
          base::UTF16ToUTF8(related_application.platform.value()));
    }
    CHECK(related_application.url.is_empty() ||
          related_application.url.is_valid());
    related_application_proto->set_url(related_application.url.spec());
    if (related_application.id) {
      related_application_proto->set_id(
          base::UTF16ToUTF8(related_application.id.value()));
    }
  }

  if (web_app.pending_update_info().has_value()) {
    CHECK(web_app.pending_update_info()->has_name() ||
          (!web_app.pending_update_info()->trusted_icons().empty() &&
           !web_app.pending_update_info()->manifest_icons().empty()));
    if (!web_app.pending_update_info()->manifest_icons().empty() &&
        !web_app.pending_update_info()->trusted_icons().empty()) {
      for (const auto& icon : web_app.pending_update_info()->manifest_icons()) {
        CHECK(icon.has_url() && icon.has_purpose());
      }
      for (const auto& icon : web_app.pending_update_info()->trusted_icons()) {
        CHECK(icon.has_url() && icon.has_purpose());
      }
      CHECK(
          !web_app.pending_update_info()->downloaded_manifest_icons().empty() &&
          !web_app.pending_update_info()->downloaded_trusted_icons().empty());
    }
    CHECK(web_app.pending_update_info()->has_was_ignored());
    *local_data->mutable_pending_update_info() = *web_app.pending_update_info();
  }

  for (const apps::IconInfo& trusted_icon_info : web_app.trusted_icons()) {
    *(local_data->add_trusted_icons()) =
        AppIconInfoToSyncProto(trusted_icon_info);
  }

  for (SquareSizePx size :
       web_app.stored_trusted_icon_sizes(IconPurpose::ANY)) {
    local_data->add_stored_trusted_icon_sizes_any(size);
  }
  for (SquareSizePx size :
       web_app.stored_trusted_icon_sizes(IconPurpose::MASKABLE)) {
    local_data->add_stored_trusted_icon_sizes_maskable(size);
  }

  for (const auto& pattern : web_app.borderless_url_patterns()) {
    *(local_data->add_borderless_url_patterns()) = ToUrlPatternProto(pattern);
  }

  for (const auto& installed_by_data : web_app.installed_by()) {
    *(local_data->add_installed_by()) = installed_by_data.ToProto();
  }

  return local_data;
}

}  // namespace web_app
