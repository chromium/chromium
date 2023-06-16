// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_url_pattern.pb.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/url_pattern.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/url_pattern.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#endif

namespace web_app {

namespace {

ShareTarget_Method MethodToProto(apps::ShareTarget::Method method) {
  switch (method) {
    case apps::ShareTarget::Method::kGet:
      return ShareTarget_Method_GET;
    case apps::ShareTarget::Method::kPost:
      return ShareTarget_Method_POST;
  }
}

apps::ShareTarget::Method ProtoToMethod(ShareTarget_Method method) {
  switch (method) {
    case ShareTarget_Method_GET:
      return apps::ShareTarget::Method::kGet;
    case ShareTarget_Method_POST:
      return apps::ShareTarget::Method::kPost;
  }
}

ShareTarget_Enctype EnctypeToProto(apps::ShareTarget::Enctype enctype) {
  switch (enctype) {
    case apps::ShareTarget::Enctype::kFormUrlEncoded:
      return ShareTarget_Enctype_FORM_URL_ENCODED;
    case apps::ShareTarget::Enctype::kMultipartFormData:
      return ShareTarget_Enctype_MULTIPART_FORM_DATA;
  }
}

apps::ShareTarget::Enctype ProtoToEnctype(ShareTarget_Enctype enctype) {
  switch (enctype) {
    case ShareTarget_Enctype_FORM_URL_ENCODED:
      return apps::ShareTarget::Enctype::kFormUrlEncoded;
    case ShareTarget_Enctype_MULTIPART_FORM_DATA:
      return apps::ShareTarget::Enctype::kMultipartFormData;
  }
}

blink::mojom::CaptureLinks ProtoToCaptureLinks(
    WebAppProto::CaptureLinks capture_links) {
  switch (capture_links) {
    case WebAppProto_CaptureLinks_NONE:
      return blink::mojom::CaptureLinks::kNone;
    case WebAppProto_CaptureLinks_NEW_CLIENT:
      return blink::mojom::CaptureLinks::kNewClient;
    case WebAppProto_CaptureLinks_EXISTING_CLIENT_NAVIGATE:
      return blink::mojom::CaptureLinks::kExistingClientNavigate;
  }
}

WebAppProto::CaptureLinks CaptureLinksToProto(
    blink::mojom::CaptureLinks capture_links) {
  switch (capture_links) {
    case blink::mojom::CaptureLinks::kUndefined:
      NOTREACHED();
      [[fallthrough]];
    case blink::mojom::CaptureLinks::kNone:
      return WebAppProto_CaptureLinks_NONE;
    case blink::mojom::CaptureLinks::kNewClient:
      return WebAppProto_CaptureLinks_NEW_CLIENT;
    case blink::mojom::CaptureLinks::kExistingClientNavigate:
      return WebAppProto_CaptureLinks_EXISTING_CLIENT_NAVIGATE;
  }
}

LaunchHandler::ClientMode ProtoLaunchHandlerToLaunchHandlerClientMode(
    LaunchHandlerProto::DeprecatedRouteTo route_to,
    LaunchHandlerProto::DeprecatedNavigateExistingClient
        navigate_existing_client,
    LaunchHandlerProto::ClientMode client_mode) {
  switch (client_mode) {
    case LaunchHandlerProto_ClientMode_AUTO:
      return LaunchHandler::ClientMode::kAuto;
    case LaunchHandlerProto_ClientMode_NAVIGATE_NEW:
      return LaunchHandler::ClientMode::kNavigateNew;
    case LaunchHandlerProto_ClientMode_NAVIGATE_EXISTING:
      return LaunchHandler::ClientMode::kNavigateExisting;
    case LaunchHandlerProto_ClientMode_FOCUS_EXISTING:
      return LaunchHandler::ClientMode::kFocusExisting;
    case LaunchHandlerProto_ClientMode_UNSPECIFIED_CLIENT_MODE: {
      // route_to was removed in favor of client_mode, fall back to it if client
      // mode is unset.
      switch (route_to) {
        case LaunchHandlerProto_DeprecatedRouteTo_UNSPECIFIED_ROUTE:
        case LaunchHandlerProto_DeprecatedRouteTo_AUTO_ROUTE:
          return LaunchHandler::ClientMode::kAuto;
        case LaunchHandlerProto_DeprecatedRouteTo_NEW_CLIENT:
          return LaunchHandler::ClientMode::kNavigateNew;
        case LaunchHandlerProto_DeprecatedRouteTo_EXISTING_CLIENT:
          // route_to: existing-client and navigate_existing_client were removed
          // in favor of existing-client-navigate and existing-client-retain.
          if (navigate_existing_client ==
              LaunchHandlerProto_DeprecatedNavigateExistingClient_NEVER) {
            return LaunchHandler::ClientMode::kFocusExisting;
          }
          return LaunchHandler::ClientMode::kNavigateExisting;
        case LaunchHandlerProto_DeprecatedRouteTo_EXISTING_CLIENT_NAVIGATE:
          return LaunchHandler::ClientMode::kNavigateExisting;
        case LaunchHandlerProto_DeprecatedRouteTo_EXISTING_CLIENT_RETAIN:
          return LaunchHandler::ClientMode::kFocusExisting;
      }
    }
  }
}

LaunchHandlerProto::ClientMode LaunchHandlerClientModeToProto(
    LaunchHandler::ClientMode client_mode) {
  switch (client_mode) {
    case LaunchHandler::ClientMode::kAuto:
      return LaunchHandlerProto_ClientMode_AUTO;
    case LaunchHandler::ClientMode::kNavigateNew:
      return LaunchHandlerProto_ClientMode_NAVIGATE_NEW;
    case LaunchHandler::ClientMode::kNavigateExisting:
      return LaunchHandlerProto_ClientMode_NAVIGATE_EXISTING;
    case LaunchHandler::ClientMode::kFocusExisting:
      return LaunchHandlerProto_ClientMode_FOCUS_EXISTING;
  }
}

ApiApprovalState ProtoToApiApprovalState(
    WebAppProto::ApiApprovalState approval_state) {
  switch (approval_state) {
    case WebAppProto_ApiApprovalState_REQUIRES_PROMPT:
      return ApiApprovalState::kRequiresPrompt;
    case WebAppProto_ApiApprovalState_ALLOWED:
      return ApiApprovalState::kAllowed;
    case WebAppProto_ApiApprovalState_DISALLOWED:
      return ApiApprovalState::kDisallowed;
  }
}

WebAppProto::ApiApprovalState ApiApprovalStateToProto(
    ApiApprovalState approval_state) {
  switch (approval_state) {
    case ApiApprovalState::kRequiresPrompt:
      return WebAppProto_ApiApprovalState_REQUIRES_PROMPT;
    case ApiApprovalState::kAllowed:
      return WebAppProto_ApiApprovalState_ALLOWED;
    case ApiApprovalState::kDisallowed:
      return WebAppProto_ApiApprovalState_DISALLOWED;
  }
}

OsIntegrationState ProtoToOsIntegrationState(
    WebAppProto::OsIntegrationState state) {
  switch (state) {
    case WebAppProto_OsIntegrationState_ENABLED:
      return OsIntegrationState::kEnabled;
    case WebAppProto_OsIntegrationState_DISABLED:
      return OsIntegrationState::kDisabled;
  }
}

WebAppProto::OsIntegrationState OsIntegrationStateToProto(
    OsIntegrationState state) {
  switch (state) {
    case OsIntegrationState::kEnabled:
      return WebAppProto_OsIntegrationState_ENABLED;
    case OsIntegrationState::kDisabled:
      return WebAppProto_OsIntegrationState_DISABLED;
  }
}

apps::FileHandler::LaunchType ProtoToLaunchType(
    WebAppFileHandlerProto::LaunchType state) {
  switch (state) {
    case WebAppFileHandlerProto_LaunchType_SINGLE_CLIENT:
      return apps::FileHandler::LaunchType::kSingleClient;
    case WebAppFileHandlerProto_LaunchType_MULTIPLE_CLIENTS:
      return apps::FileHandler::LaunchType::kMultipleClients;
    case WebAppFileHandlerProto_LaunchType_UNDEFINED:
      return apps::FileHandler::LaunchType::kSingleClient;
  }
}

WebAppFileHandlerProto::LaunchType LaunchTypeToProto(
    apps::FileHandler::LaunchType state) {
  switch (state) {
    case apps::FileHandler::LaunchType::kSingleClient:
      return WebAppFileHandlerProto_LaunchType_SINGLE_CLIENT;
    case apps::FileHandler::LaunchType::kMultipleClients:
      return WebAppFileHandlerProto_LaunchType_MULTIPLE_CLIENTS;
  }
}

WebAppManagement::Type ProtoToWebAppManagement(WebAppManagementProto type) {
  switch (type) {
    case WebAppManagementProto::WEBAPPMANAGEMENT_UNSPECIFIED:
      NOTREACHED();
      [[fallthrough]];
    case WebAppManagementProto::SYSTEM:
      return WebAppManagement::Type::kSystem;
    case WebAppManagementProto::KIOSK:
      return WebAppManagement::Type::kKiosk;
    case WebAppManagementProto::POLICY:
      return WebAppManagement::Type::kPolicy;
    case WebAppManagementProto::SUBAPP:
      return WebAppManagement::Type::kSubApp;
    case WebAppManagementProto::WEBAPPSTORE:
      return WebAppManagement::Type::kWebAppStore;
    case WebAppManagementProto::SYNC:
      return WebAppManagement::Type::kSync;
    case WebAppManagementProto::DEFAULT:
      return WebAppManagement::Type::kDefault;
    case WebAppManagementProto::COMMAND_LINE:
      return WebAppManagement::Type::kCommandLine;
    case WebAppManagementProto::OEM:
      return WebAppManagement::Type::kOem;
    case WebAppManagementProto::ONEDRIVEINTEGRATION:
      return WebAppManagement::Type::kOneDriveIntegration;
  }
}

WebAppManagementProto WebAppManagementToProto(WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kSystem:
      return WebAppManagementProto::SYSTEM;
    case WebAppManagement::Type::kKiosk:
      return WebAppManagementProto::KIOSK;
    case WebAppManagement::Type::kPolicy:
      return WebAppManagementProto::POLICY;
    case WebAppManagement::Type::kSubApp:
      return WebAppManagementProto::SUBAPP;
    case WebAppManagement::Type::kWebAppStore:
      return WebAppManagementProto::WEBAPPSTORE;
    case WebAppManagement::Type::kSync:
      return WebAppManagementProto::SYNC;
    case WebAppManagement::Type::kDefault:
      return WebAppManagementProto::DEFAULT;
    case WebAppManagement::Type::kCommandLine:
      return WebAppManagementProto::COMMAND_LINE;
    case WebAppManagement::Type::kOem:
      return WebAppManagementProto::OEM;
    case WebAppManagement::Type::kOneDriveIntegration:
      return WebAppManagementProto::ONEDRIVEINTEGRATION;
  }
}

proto::TabStrip::Visibility TabStripVisibilityToProto(
    TabStrip::Visibility visibility) {
  switch (visibility) {
    case TabStrip::Visibility::kAuto:
      return proto::TabStrip_Visibility_AUTO;
    case TabStrip::Visibility::kAbsent:
      return proto::TabStrip_Visibility_ABSENT;
  }
}

std::string FilePathToProto(const base::FilePath& path) {
  base::Pickle pickle;
  path.WriteToPickle(&pickle);
  return std::string(pickle.data_as_char(), pickle.size());
}

absl::optional<base::FilePath> ProtoToFilePath(const std::string& bytes) {
  const base::Pickle pickle(bytes.data(), bytes.size());
  base::PickleIterator pickle_iterator(pickle);

  base::FilePath path;
  if (!path.ReadFromPickle(&pickle_iterator)) {
    return absl::nullopt;
  }
  return path;
}

template <typename T>
void IsolatedWebAppLocationToProto(const IsolatedWebAppLocation& location,
                                   T* proto) {
  absl::visit(base::Overloaded{
                  [&proto](const InstalledBundle& bundle) {
                    proto->mutable_installed_bundle()->set_path(
                        FilePathToProto(bundle.path));
                  },
                  [&proto](const DevModeBundle& bundle) {
                    proto->mutable_dev_mode_bundle()->set_path(
                        FilePathToProto(bundle.path));
                  },
                  [&proto](const DevModeProxy& proxy) {
                    DCHECK(!proxy.proxy_url.opaque());
                    proto->mutable_dev_mode_proxy()->set_proxy_url(
                        proxy.proxy_url.Serialize());
                  },
              },
              location);
}

template <typename T>
base::expected<IsolatedWebAppLocation, std::string>
ProtoToIsolatedWebAppLocation(const T& proto) {
  switch (proto.location_case()) {
    case T::LocationCase::kInstalledBundle: {
      absl::optional<base::FilePath> path =
          ProtoToFilePath(proto.installed_bundle().path());
      if (!path.has_value()) {
        return base::unexpected(
            ".installed_bundle.path parse error: cannot deserialize file path");
      }
      return InstalledBundle{.path = *path};
    }

    case T::LocationCase::kDevModeBundle: {
      absl::optional<base::FilePath> path =
          ProtoToFilePath(proto.dev_mode_bundle().path());
      if (!path.has_value()) {
        return base::unexpected(
            ".dev_mode_bundle.path parse error: cannot deserialize file path");
      }
      return DevModeBundle{.path = *path};
    }

    case T::LocationCase::kDevModeProxy: {
      GURL gurl_proxy_url = GURL(proto.dev_mode_proxy().proxy_url());
      url::Origin proxy_url = url::Origin::Create(gurl_proxy_url);
      if (!gurl_proxy_url.is_valid() || proxy_url.opaque()) {
        return base::unexpected(
            ".dev_mode_proxy.proxy_url parse error: cannot deserialize proxy "
            "url. Value: " +
            proto.dev_mode_proxy().proxy_url());
      }
      return DevModeProxy{.proxy_url = proxy_url};
    }

    case T::LocationCase::LOCATION_NOT_SET:
      return base::unexpected(" parse error: not set");
  }
}

}  // anonymous namespace

WebAppDatabase::WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory,
                               ReportErrorCallback error_callback)
    : database_factory_(database_factory),
      error_callback_(std::move(error_callback)) {
  DCHECK(database_factory_);
}

WebAppDatabase::~WebAppDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebAppDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_);

  syncer::OnceModelTypeStoreFactory store_factory =
      database_factory_->GetStoreFactory();

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebAppDatabase::OnDatabaseOpened,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(opened_);

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // |update_data| can be empty here but we should write |metadata_change_list|
  // anyway.
  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const AppId& app_id : update_data.apps_to_delete)
    write_batch->DeleteData(app_id);

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&WebAppDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
std::unique_ptr<WebAppProto> WebAppDatabase::CreateWebAppProto(
    const WebApp& web_app) {
  auto local_data = std::make_unique<WebAppProto>();

  // Required fields:
  const GURL start_url = web_app.start_url();
  DCHECK(start_url.is_valid());

  DCHECK(!web_app.app_id().empty());
  DCHECK(web_app.manifest_id().is_valid());

  // Set sync data to sync proto.
  *(local_data->mutable_sync_data()) = WebAppToSyncProto(web_app);

  local_data->set_name(web_app.untranslated_name());

  DCHECK(web_app.sources_.any() || web_app.is_uninstalling());
  local_data->mutable_sources()->set_system(
      web_app.sources_[WebAppManagement::kSystem]);
  local_data->mutable_sources()->set_policy(
      web_app.sources_[WebAppManagement::kPolicy]);
  local_data->mutable_sources()->set_web_app_store(
      web_app.sources_[WebAppManagement::kWebAppStore]);
  local_data->mutable_sources()->set_sync(
      web_app.sources_[WebAppManagement::kSync]);
  local_data->mutable_sources()->set_default_(
      web_app.sources_[WebAppManagement::kDefault]);
  local_data->mutable_sources()->set_sub_app(
      web_app.sources_[WebAppManagement::kSubApp]);
  local_data->mutable_sources()->set_kiosk(
      web_app.sources_[WebAppManagement::kKiosk]);
  local_data->mutable_sources()->set_command_line(
      web_app.sources_[WebAppManagement::kCommandLine]);
  local_data->mutable_sources()->set_oem(
      web_app.sources_[WebAppManagement::kOem]);
  local_data->mutable_sources()->set_one_drive_integration(
      web_app.sources_[WebAppManagement::kOneDriveIntegration]);

  local_data->set_is_locally_installed(web_app.is_locally_installed());

  // Optional fields:
  if (web_app.launch_query_params())
    local_data->set_launch_query_params(*web_app.launch_query_params());

  if (web_app.display_mode() != DisplayMode::kUndefined) {
    local_data->set_display_mode(
        ToWebAppProtoDisplayMode(web_app.display_mode()));
  }

  for (const DisplayMode& display_mode : web_app.display_mode_override()) {
    local_data->add_display_mode_override(
        ToWebAppProtoDisplayMode(display_mode));
  }

  local_data->set_description(web_app.untranslated_description());
  if (!web_app.scope().is_empty())
    local_data->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    local_data->set_theme_color(web_app.theme_color().value());
  if (web_app.dark_mode_theme_color().has_value())
    local_data->set_dark_mode_theme_color(
        web_app.dark_mode_theme_color().value());
  if (web_app.background_color().has_value())
    local_data->set_background_color(web_app.background_color().value());
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
  if (!web_app.install_time().is_null()) {
    local_data->set_install_time(
        syncer::TimeToProtoTime(web_app.install_time()));
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
    mutable_chromeos_data->set_show_in_search(chromeos_data.show_in_search);
    mutable_chromeos_data->set_show_in_management(
        chromeos_data.show_in_management);
    mutable_chromeos_data->set_is_disabled(chromeos_data.is_disabled);
    mutable_chromeos_data->set_oem_installed(chromeos_data.oem_installed);
    mutable_chromeos_data->set_handles_file_open_intents(
        chromeos_data.handles_file_open_intents);
    if (chromeos_data.app_profile_path.has_value()) {
      CHECK(!chromeos_data.app_profile_path.value().empty());
      mutable_chromeos_data->set_app_profile_path(
          FilePathToProto(chromeos_data.app_profile_path.value()));
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  if (web_app.run_on_os_login_os_integration_state()) {
    local_data->set_run_on_os_login_os_integration_state(
        ToWebAppProtoRunOnOsLoginMode(
            *web_app.run_on_os_login_os_integration_state()));
  }
  local_data->set_is_from_sync_and_pending_installation(
      web_app.is_from_sync_and_pending_installation());
  local_data->set_is_uninstalling(web_app.is_uninstalling());

  for (const apps::IconInfo& icon_info : web_app.manifest_icons())
    *(local_data->add_manifest_icons()) = AppIconInfoToSyncProto(icon_info);

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
    WebAppFileHandlerProto* file_handler_proto =
        local_data->add_file_handlers();
    DCHECK(file_handler.action.is_valid());
    file_handler_proto->set_action(file_handler.action.spec());
    file_handler_proto->set_display_name(
        base::UTF16ToUTF8(file_handler.display_name));
    file_handler_proto->set_launch_type(
        LaunchTypeToProto(file_handler.launch_type));

    for (const auto& accept_entry : file_handler.accept) {
      WebAppFileHandlerAcceptProto* accept_entry_proto =
          file_handler_proto->add_accept();
      accept_entry_proto->set_mimetype(accept_entry.mime_type);

      for (const auto& file_extension : accept_entry.file_extensions)
        accept_entry_proto->add_file_extensions(file_extension);
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
    if (!params.title.empty())
      mutable_share_target_params->set_title(params.title);
    if (!params.text.empty())
      mutable_share_target_params->set_text(params.text);
    if (!params.url.empty())
      mutable_share_target_params->set_url(params.url);

    for (const auto& files_entry : params.files) {
      ShareTargetParamsFile* mutable_share_target_files =
          mutable_share_target_params->add_files();
      mutable_share_target_files->set_name(files_entry.name);

      for (const auto& file_type : files_entry.accept)
        mutable_share_target_files->add_accept(file_type);
    }
  }

  for (const WebAppShortcutsMenuItemInfo& shortcut_info :
       web_app.shortcuts_menu_item_infos()) {
    WebAppShortcutsMenuItemInfoProto* shortcut_info_proto =
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
  }

  for (const IconSizes& icon_sizes :
       web_app.downloaded_shortcuts_menu_icons_sizes()) {
    DownloadedShortcutsMenuIconSizesProto* icon_sizes_proto =
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
    WebAppProtocolHandler* protocol_handler_proto =
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

  for (const auto& url_handler : web_app.url_handlers()) {
    WebAppUrlHandlerProto* url_handler_proto = local_data->add_url_handlers();
    url_handler_proto->set_origin(url_handler.origin.Serialize());
    url_handler_proto->set_has_origin_wildcard(url_handler.has_origin_wildcard);
  }

  for (const auto& scope_extension : web_app.scope_extensions()) {
    WebAppScopeExtensionProto* scope_extension_proto =
        local_data->add_scope_extensions();
    scope_extension_proto->set_origin(scope_extension.origin.Serialize());
    scope_extension_proto->set_has_origin_wildcard(
        scope_extension.has_origin_wildcard);
  }

  for (const auto& valid_extension : web_app.validated_scope_extensions()) {
    WebAppScopeExtensionProto* scope_extension_proto =
        local_data->add_scope_extensions_validated();
    scope_extension_proto->set_origin(valid_extension.origin.Serialize());
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

  if (web_app.capture_links() != blink::mojom::CaptureLinks::kUndefined)
    local_data->set_capture_links(CaptureLinksToProto(web_app.capture_links()));
  else
    local_data->clear_capture_links();

  if (!web_app.manifest_url().is_empty())
    local_data->set_manifest_url(web_app.manifest_url().spec());

  local_data->set_file_handler_approval_state(
      ApiApprovalStateToProto(web_app.file_handler_approval_state()));

  local_data->set_file_handler_os_integration_state(
      OsIntegrationStateToProto(web_app.file_handler_os_integration_state()));

  local_data->set_window_controls_overlay_enabled(
      web_app.window_controls_overlay_enabled());

  if (web_app.launch_handler()) {
    local_data->mutable_launch_handler()->set_client_mode(
        LaunchHandlerClientModeToProto(web_app.launch_handler()->client_mode));
  }

  if (web_app.parent_app_id_) {
    local_data->set_parent_app_id(*web_app.parent_app_id_);
  }

  if (!web_app.permissions_policy().empty()) {
    auto& policy = *local_data->mutable_permissions_policy();
    const auto& feature_to_name_map =
        blink::GetPermissionsPolicyFeatureToNameMap();
    for (const auto& decl : web_app.permissions_policy()) {
      WebAppPermissionsPolicy proto_policy;
      const auto feature_name = feature_to_name_map.find(decl.feature);
      if (feature_name == feature_to_name_map.end())
        continue;
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
      ManagementToExternalConfigInfo* management_config_proto =
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
    if (absl::holds_alternative<TabStrip::Visibility>(tab_strip.home_tab)) {
      mutable_tab_strip->set_home_tab_visibility(TabStripVisibilityToProto(
          absl::get<TabStrip::Visibility>(tab_strip.home_tab)));
    } else {
      auto* mutable_home_tab_params =
          mutable_tab_strip->mutable_home_tab_params();

      const absl::optional<std::vector<blink::Manifest::ImageResource>>& icons =
          absl::get<blink::Manifest::HomeTabParams>(tab_strip.home_tab).icons;
      for (const auto& image_resource : *icons) {
        *(mutable_home_tab_params->add_icons()) =
            AppImageResourceToProto(image_resource);
      }

      const std::vector<blink::UrlPattern>& scope_patterns =
          absl::get<blink::Manifest::HomeTabParams>(tab_strip.home_tab)
              .scope_patterns;
      for (const auto& pattern : scope_patterns) {
        *(mutable_home_tab_params->add_scope_patterns()) =
            ToUrlPatternProto(pattern);
      }
    }

    if (absl::holds_alternative<TabStrip::Visibility>(
            tab_strip.new_tab_button)) {
      mutable_tab_strip->set_new_tab_button_visibility(
          TabStripVisibilityToProto(absl::get<TabStrip::Visibility>(
              web_app.tab_strip().value().new_tab_button)));
    } else {
      auto* mutable_new_tab_button_params =
          mutable_tab_strip->mutable_new_tab_button_params();
      absl::optional<GURL> url = absl::get<blink::Manifest::NewTabButtonParams>(
                                     tab_strip.new_tab_button)
                                     .url;
      if (url)
        mutable_new_tab_button_params->set_url(url.value().spec());
    }
  }

  *local_data->mutable_current_os_integration_states() =
      web_app.current_os_integration_states();

  if (web_app.app_size_in_bytes().has_value())
    local_data->set_app_size_in_bytes(web_app.app_size_in_bytes().value());

  if (web_app.data_size_in_bytes().has_value())
    local_data->set_data_size_in_bytes(web_app.data_size_in_bytes().value());

  local_data->set_always_show_toolbar_in_fullscreen(
      web_app.always_show_toolbar_in_fullscreen());

  if (web_app.isolation_data().has_value()) {
    auto* mutable_data = local_data->mutable_isolation_data();

    IsolatedWebAppLocationToProto(web_app.isolation_data()->location,
                                  mutable_data);
    mutable_data->set_version(web_app.isolation_data()->version.GetString());
    for (const std::string& partition :
         web_app.isolation_data()->controlled_frame_partitions) {
      mutable_data->add_controlled_frame_partitions(partition);
    }

    if (web_app.isolation_data()->pending_update_info().has_value()) {
      const WebApp::IsolationData::PendingUpdateInfo& pending_update_info =
          *web_app.isolation_data()->pending_update_info();
      auto* mutable_pending_update_info =
          mutable_data->mutable_pending_update_info();

      IsolatedWebAppLocationToProto(pending_update_info.location,
                                    mutable_pending_update_info);
      mutable_pending_update_info->set_version(
          pending_update_info.version.GetString());
    }
  }

  return local_data;
}

// static
std::unique_ptr<WebApp> WebAppDatabase::CreateWebApp(
    const WebAppProto& local_data) {
  if (!local_data.has_sync_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no sync_data field";
    return nullptr;
  }

  const sync_pb::WebAppSpecifics& sync_data = local_data.sync_data();

  // AppId is a hash of start_url. Read start_url first:
  GURL start_url(sync_data.start_url());
  if (start_url.is_empty() || !start_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto start_url parse error: "
                << start_url.possibly_invalid_spec();
    return nullptr;
  }

  ManifestId manifest_id;
  if (sync_data.has_relative_manifest_id()) {
    manifest_id =
        GenerateManifestId(sync_data.relative_manifest_id(), start_url);
  } else {
    manifest_id = GenerateManifestIdFromStartUrlOnly(start_url);
  }

  AppId app_id = GenerateAppIdFromManifestId(manifest_id);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->SetManifestId(manifest_id);

  // Required fields:
  if (!local_data.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    return nullptr;
  }

  WebAppSources sources;
  sources[WebAppManagement::kSystem] = local_data.sources().system();
  sources[WebAppManagement::kPolicy] = local_data.sources().policy();
  sources[WebAppManagement::kWebAppStore] =
      local_data.sources().web_app_store();
  sources[WebAppManagement::kSync] = local_data.sources().sync();
  sources[WebAppManagement::kDefault] = local_data.sources().default_();
  sources[WebAppManagement::kOem] = local_data.sources().oem();
  if (local_data.sources().has_sub_app()) {
    sources[WebAppManagement::kSubApp] = local_data.sources().sub_app();
  }
  if (local_data.sources().has_kiosk()) {
    sources[WebAppManagement::kKiosk] = local_data.sources().kiosk();
  }
  if (local_data.sources().has_command_line()) {
    sources[WebAppManagement::kCommandLine] =
        local_data.sources().command_line();
  }
  if (local_data.sources().has_one_drive_integration()) {
    sources[WebAppManagement::kOneDriveIntegration] =
        local_data.sources().one_drive_integration();
  }
  if (!sources.any() && !local_data.is_uninstalling()) {
    DLOG(ERROR) << "WebApp proto parse error: no any source in sources field, "
                   "and is_uninstalling isn't true.";
    return nullptr;
  }
  web_app->sources_ = sources;

  if (!local_data.has_name()) {
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(local_data.name());

  if (!sync_data.has_user_display_mode()) {
    DLOG(ERROR) << "WebApp proto parse error: no user_display_mode field";
    return nullptr;
  }
  web_app->SetUserDisplayMode(
      CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
          sync_data.user_display_mode()));

  // Ordinals used for chrome://apps page.
  syncer::StringOrdinal page_ordinal =
      syncer::StringOrdinal(sync_data.user_page_ordinal());
  if (!page_ordinal.IsValid())
    page_ordinal = syncer::StringOrdinal();
  syncer::StringOrdinal launch_ordinal =
      syncer::StringOrdinal(sync_data.user_launch_ordinal());
  if (!launch_ordinal.IsValid())
    launch_ordinal = syncer::StringOrdinal();
  web_app->SetUserPageOrdinal(page_ordinal);
  web_app->SetUserLaunchOrdinal(launch_ordinal);

  if (!local_data.has_is_locally_installed()) {
    DLOG(ERROR) << "WebApp proto parse error: no is_locally_installed field";
    return nullptr;
  }
  web_app->SetIsLocallyInstalled(local_data.is_locally_installed());

  auto& chromeos_data_proto = local_data.chromeos_data();

  if (IsChromeOsDataMandatory() && !local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no chromeos_data field. The web "
                << "app might have been installed when running on an OS other "
                << "than Chrome OS.";
    return nullptr;
  }

  if (!IsChromeOsDataMandatory() && local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: has chromeos_data field. The web "
                << "app might have been installed when running on Chrome OS.";
    return nullptr;
  }

  if (local_data.has_chromeos_data()) {
    auto chromeos_data = absl::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = chromeos_data_proto.show_in_launcher();
    chromeos_data->show_in_search = chromeos_data_proto.show_in_search();
    chromeos_data->show_in_management =
        chromeos_data_proto.show_in_management();
    chromeos_data->is_disabled = chromeos_data_proto.is_disabled();
    chromeos_data->oem_installed = chromeos_data_proto.oem_installed();
    chromeos_data->handles_file_open_intents =
        chromeos_data_proto.handles_file_open_intents();
    if (chromeos_data_proto.has_app_profile_path()) {
      auto parsed_path =
          ProtoToFilePath(chromeos_data_proto.app_profile_path());
      CHECK(parsed_path.has_value());
      CHECK(!parsed_path.value().empty());
      chromeos_data->app_profile_path = std::move(parsed_path);
    }
    web_app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (local_data.client_data().has_system_web_app_data()) {
    ash::SystemWebAppData& swa_data =
        web_app->client_data()->system_web_app_data.emplace();

    swa_data.system_app_type = static_cast<ash::SystemWebAppType>(
        local_data.client_data().system_web_app_data().system_app_type());
  }
#endif

  // Optional fields:
  if (local_data.has_launch_query_params())
    web_app->SetLaunchQueryParams(local_data.launch_query_params());

  if (local_data.has_display_mode())
    web_app->SetDisplayMode(ToMojomDisplayMode(local_data.display_mode()));

  std::vector<DisplayMode> display_mode_override;
  for (int i = 0; i < local_data.display_mode_override_size(); i++) {
    WebAppProto::DisplayMode display_mode = local_data.display_mode_override(i);
    display_mode_override.push_back(ToMojomDisplayMode(display_mode));
  }
  web_app->SetDisplayModeOverride(std::move(display_mode_override));

  if (local_data.has_description())
    web_app->SetDescription(local_data.description());

  if (local_data.has_scope()) {
    GURL scope(local_data.scope());
    if (scope.is_empty() || !scope.is_valid()) {
      DLOG(ERROR) << "WebApp proto scope parse error: "
                  << scope.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetScope(scope);
  }

  if (local_data.has_theme_color()) {
    web_app->SetThemeColor(local_data.theme_color());
  }

  if (local_data.has_dark_mode_theme_color()) {
    web_app->SetDarkModeThemeColor(local_data.dark_mode_theme_color());
  }

  if (local_data.has_background_color()) {
    web_app->SetBackgroundColor(local_data.background_color());
  }

  if (local_data.has_dark_mode_background_color()) {
    web_app->SetDarkModeBackgroundColor(
        local_data.dark_mode_background_color());
  }

  if (local_data.has_is_from_sync_and_pending_installation())
    web_app->SetIsFromSyncAndPendingInstallation(
        local_data.is_from_sync_and_pending_installation());

  if (local_data.has_is_uninstalling())
    web_app->SetIsUninstalling(local_data.is_uninstalling());

  if (local_data.has_last_badging_time()) {
    web_app->SetLastBadgingTime(
        syncer::ProtoTimeToTime(local_data.last_badging_time()));
  }
  if (local_data.has_last_launch_time()) {
    web_app->SetLastLaunchTime(
        syncer::ProtoTimeToTime(local_data.last_launch_time()));
  }
  if (local_data.has_latest_install_source()) {
    int install_source = local_data.latest_install_source();
    if (install_source >= 0 &&
        install_source <
            static_cast<int>(webapps::WebappInstallSource::COUNT)) {
      web_app->SetLatestInstallSource(
          static_cast<webapps::WebappInstallSource>(install_source));
    }
  }
  if (local_data.has_manifest_update_time()) {
    web_app->SetManifestUpdateTime(
        syncer::ProtoTimeToTime(local_data.manifest_update_time()));
  }

  if (local_data.has_install_time()) {
    web_app->SetInstallTime(syncer::ProtoTimeToTime(local_data.install_time()));
  }

  absl::optional<WebApp::SyncFallbackData> parsed_sync_fallback_data =
      ParseSyncFallbackDataStruct(sync_data);
  if (!parsed_sync_fallback_data.has_value()) {
    // ParseSyncFallbackDataStruct() reports any errors.
    return nullptr;
  }
  web_app->SetSyncFallbackData(std::move(parsed_sync_fallback_data.value()));

  absl::optional<std::vector<apps::IconInfo>> parsed_manifest_icons =
      ParseAppIconInfos("WebApp", local_data.manifest_icons());
  if (!parsed_manifest_icons) {
    // ParseWebAppIconInfos() reports any errors.
    return nullptr;
  }
  web_app->SetManifestIcons(std::move(parsed_manifest_icons.value()));

  std::vector<SquareSizePx> icon_sizes_any;
  for (int32_t size : local_data.downloaded_icon_sizes_purpose_any())
    icon_sizes_any.push_back(size);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY,
                                  SortedSizesPx(std::move(icon_sizes_any)));

  std::vector<SquareSizePx> icon_sizes_maskable;
  for (int32_t size : local_data.downloaded_icon_sizes_purpose_maskable())
    icon_sizes_maskable.push_back(size);
  web_app->SetDownloadedIconSizes(
      IconPurpose::MASKABLE, SortedSizesPx(std::move(icon_sizes_maskable)));

  std::vector<SquareSizePx> icon_sizes_monochrome;
  for (int32_t size : local_data.downloaded_icon_sizes_purpose_monochrome())
    icon_sizes_monochrome.push_back(size);
  web_app->SetDownloadedIconSizes(
      IconPurpose::MONOCHROME, SortedSizesPx(std::move(icon_sizes_monochrome)));

  web_app->SetIsGeneratedIcon(local_data.is_generated_icon());

  apps::FileHandlers file_handlers;
  for (const auto& file_handler_proto : local_data.file_handlers()) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(file_handler_proto.action());

    if (file_handler.action.is_empty() || !file_handler.action.is_valid()) {
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

    absl::optional<std::vector<apps::IconInfo>> file_handler_icon_infos =
        ParseAppIconInfos("WebApp", file_handler_proto.downloaded_icons());
    if (!file_handler_icon_infos) {
      // ParseAppIconInfos() reports any errors.
      return nullptr;
    }
    file_handler.downloaded_icons = std::move(file_handler_icon_infos.value());

    file_handlers.push_back(std::move(file_handler));
  }
  web_app->SetFileHandlers(std::move(file_handlers));

  if (local_data.has_share_target()) {
    apps::ShareTarget share_target;
    const ShareTarget& local_share_target = local_data.share_target();
    const ShareTargetParams& local_share_target_params =
        local_share_target.params();

    GURL action(local_share_target.action());
    if (action.is_empty() || !action.is_valid()) {
      DLOG(ERROR) << "WebApp proto action parse error: "
                  << action.possibly_invalid_spec();
      return nullptr;
    }

    share_target.action = action;
    share_target.method = ProtoToMethod(local_share_target.method());
    share_target.enctype = ProtoToEnctype(local_share_target.enctype());

    if (local_share_target_params.has_title())
      share_target.params.title = local_share_target_params.title();
    if (local_share_target_params.has_text())
      share_target.params.text = local_share_target_params.text();
    if (local_share_target_params.has_url())
      share_target.params.url = local_share_target_params.url();

    for (const auto& share_target_params_file :
         local_share_target_params.files()) {
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
  for (const auto& shortcut_info_proto :
       local_data.shortcuts_menu_item_infos()) {
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
       local_data.downloaded_shortcuts_menu_icons_sizes()) {
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
    DLOG(ERROR) << "WebApp proto had more downloaded shortcut icons than infos";
    return nullptr;
  }
  web_app->SetShortcutsMenuInfo(std::move(shortcuts_menu_item_infos),
                                std::move(shortcuts_menu_icons_sizes));

  std::vector<std::string> additional_search_terms;
  for (const std::string& additional_search_term :
       local_data.additional_search_terms()) {
    if (additional_search_term.empty()) {
      DLOG(ERROR) << "WebApp AdditionalSearchTerms proto action parse error";
      return nullptr;
    }
    additional_search_terms.push_back(additional_search_term);
  }
  web_app->SetAdditionalSearchTerms(std::move(additional_search_terms));

  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;
  for (const auto& protocol_handler_proto : local_data.protocol_handlers()) {
    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol = protocol_handler_proto.protocol();
    GURL protocol_handler_url(protocol_handler_proto.url());
    if (protocol_handler_url.is_empty() || !protocol_handler_url.is_valid()) {
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
       local_data.allowed_launch_protocols()) {
    if (allowed_launch_protocol.empty()) {
      DLOG(ERROR) << "WebApp AllowedLaunchProtocols proto action parse error";
      return nullptr;
    }
    allowed_launch_protocols.push_back(allowed_launch_protocol);
  }
  web_app->SetAllowedLaunchProtocols(std::move(allowed_launch_protocols));

  std::vector<std::string> disallowed_launch_protocols;
  for (const std::string& disallowed_launch_protocol :
       local_data.disallowed_launch_protocols()) {
    if (disallowed_launch_protocol.empty()) {
      DLOG(ERROR)
          << "WebApp DisallowedLaunchProtocols proto action parse error";
      return nullptr;
    }
    disallowed_launch_protocols.push_back(disallowed_launch_protocol);
  }
  web_app->SetDisallowedLaunchProtocols(std::move(disallowed_launch_protocols));

  std::vector<apps::UrlHandlerInfo> url_handlers;
  for (const auto& url_handler_proto : local_data.url_handlers()) {
    apps::UrlHandlerInfo url_handler;

    url::Origin origin = url::Origin::Create(GURL(url_handler_proto.origin()));
    if (origin.opaque()) {
      DLOG(ERROR) << "WebApp UrlHandler proto url parse error: "
                  << origin.GetDebugString();
      return nullptr;
    }
    url_handler.origin = std::move(origin);
    url_handler.has_origin_wildcard = url_handler_proto.has_origin_wildcard();
    url_handlers.push_back(std::move(url_handler));
  }
  web_app->SetUrlHandlers(std::move(url_handlers));

  base::flat_set<ScopeExtensionInfo> scope_extensions;
  for (const auto& scope_extension_proto : local_data.scope_extensions()) {
    ScopeExtensionInfo scope_extension;

    url::Origin origin =
        url::Origin::Create(GURL(scope_extension_proto.origin()));
    if (origin.opaque()) {
      DLOG(ERROR) << "WebApp ScopeExtension proto url parse error: "
                  << origin.GetDebugString();
      return nullptr;
    }
    scope_extension.origin = std::move(origin);
    scope_extension.has_origin_wildcard =
        scope_extension_proto.has_origin_wildcard();

    scope_extensions.insert(std::move(scope_extension));
  }
  web_app->SetScopeExtensions(std::move(scope_extensions));

  base::flat_set<ScopeExtensionInfo> valid_scope_extensions;
  for (const auto& scope_extension_proto :
       local_data.scope_extensions_validated()) {
    ScopeExtensionInfo scope_extension;

    url::Origin origin =
        url::Origin::Create(GURL(scope_extension_proto.origin()));
    if (origin.opaque()) {
      DLOG(ERROR) << "WebApp ScopeExtension proto url parse error: "
                  << origin.GetDebugString();
      return nullptr;
    }
    scope_extension.origin = std::move(origin);
    scope_extension.has_origin_wildcard =
        scope_extension_proto.has_origin_wildcard();

    valid_scope_extensions.insert(std::move(scope_extension));
  }
  web_app->SetValidatedScopeExtensions(std::move(valid_scope_extensions));

  if (local_data.has_lock_screen_start_url()) {
    web_app->SetLockScreenStartUrl(GURL(local_data.lock_screen_start_url()));
  }

  if (local_data.has_note_taking_new_note_url()) {
    web_app->SetNoteTakingNewNoteUrl(
        GURL(local_data.note_taking_new_note_url()));
  }

  if (local_data.has_user_run_on_os_login_mode()) {
    web_app->SetRunOnOsLoginMode(
        ToRunOnOsLoginMode(local_data.user_run_on_os_login_mode()));
  }

  if (local_data.has_run_on_os_login_os_integration_state()) {
    web_app->SetRunOnOsLoginOsIntegrationState(
        ToRunOnOsLoginMode(local_data.run_on_os_login_os_integration_state()));
  }

  if (local_data.has_capture_links())
    web_app->SetCaptureLinks(ProtoToCaptureLinks(local_data.capture_links()));
  else
    web_app->SetCaptureLinks(blink::mojom::CaptureLinks::kUndefined);

  if (local_data.has_manifest_url()) {
    GURL manifest_url(local_data.manifest_url());
    if (manifest_url.is_empty() || !manifest_url.is_valid()) {
      DLOG(ERROR) << "WebApp proto manifest_url parse error: "
                  << manifest_url.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetManifestUrl(manifest_url);
  }

  if (local_data.has_file_handler_approval_state()) {
    web_app->SetFileHandlerApprovalState(
        ProtoToApiApprovalState(local_data.file_handler_approval_state()));
  }

  if (local_data.has_file_handler_os_integration_state()) {
    web_app->SetFileHandlerOsIntegrationState(ProtoToOsIntegrationState(
        local_data.file_handler_os_integration_state()));
  }

  if (local_data.has_window_controls_overlay_enabled()) {
    web_app->SetWindowControlsOverlayEnabled(
        local_data.window_controls_overlay_enabled());
  }

  if (local_data.has_launch_handler()) {
    const LaunchHandlerProto& launch_handler_proto =
        local_data.launch_handler();
    web_app->SetLaunchHandler(
        LaunchHandler{ProtoLaunchHandlerToLaunchHandlerClientMode(
            launch_handler_proto.route_to(),
            launch_handler_proto.navigate_existing_client(),
            launch_handler_proto.client_mode())});
  }

  if (local_data.has_parent_app_id()) {
    web_app->parent_app_id_ = local_data.parent_app_id();
  }

  if (local_data.permissions_policy_size()) {
    blink::ParsedPermissionsPolicy policy;
    const auto& name_to_feature_map =
        blink::GetPermissionsPolicyNameToFeatureMap();
    for (const auto& decl_proto : local_data.permissions_policy()) {
      blink::ParsedPermissionsPolicyDeclaration decl;
      const auto feature_enum = name_to_feature_map.find(decl_proto.feature());
      if (feature_enum == name_to_feature_map.end())
        continue;
      decl.feature = feature_enum->second;

      for (const std::string& origin : decl_proto.allowed_origins()) {
        absl::optional<blink::OriginWithPossibleWildcards>
            maybe_origin_with_possible_wildcards =
                blink::OriginWithPossibleWildcards::Parse(
                    origin,
                    blink::OriginWithPossibleWildcards::NodeType::kHeader);
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
       local_data.management_to_external_config_info()) {
    WebApp::ExternalManagementConfig config;
    base::flat_set<GURL> install_urls;
    for (const auto& install_url_proto : management_proto.install_urls()) {
      GURL install_url(install_url_proto);
      if (install_url.is_empty() || !install_url.is_valid()) {
        DLOG(ERROR) << "WebApp proto install_url parse error: "
                    << install_url.possibly_invalid_spec();
        return nullptr;
      }
      install_urls.emplace(install_url);
    }
    base::flat_set<std::string> additional_policy_ids;
    for (const auto& policy_id : management_proto.additional_policy_ids()) {
      if (policy_id.empty()) {
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

  if (local_data.has_tab_strip()) {
    web_app->SetTabStrip(ProtoToTabStrip(local_data.tab_strip()));
  }

  if (local_data.has_current_os_integration_states()) {
    web_app->SetCurrentOsIntegrationStates(
        local_data.current_os_integration_states());
  }

  if (local_data.has_app_size_in_bytes()) {
    web_app->SetAppSizeInBytes(local_data.app_size_in_bytes());
  }

  if (local_data.has_data_size_in_bytes()) {
    web_app->SetDataSizeInBytes(local_data.data_size_in_bytes());
  }

  if (local_data.has_always_show_toolbar_in_fullscreen()) {
    web_app->SetAlwaysShowToolbarInFullscreen(
        local_data.always_show_toolbar_in_fullscreen());
  }

  if (local_data.has_isolation_data()) {
    const google::protobuf::RepeatedPtrField<std::string>& partitions =
        local_data.isolation_data().controlled_frame_partitions();
    std::set<std::string> controlled_frame_partitions(partitions.begin(),
                                                      partitions.end());
    auto version_components =
        ParseIwaVersionIntoComponents(local_data.isolation_data().version());
    if (!version_components.has_value()) {
      DLOG(ERROR) << "WebApp proto isolation_data.version parse error: cannot "
                     "deserialize version: "
                  << IwaVersionParseErrorToString(version_components.error());
      return nullptr;
    }
    base::Version version(
        std::vector(version_components->begin(), version_components->end()));

    base::expected<IsolatedWebAppLocation, std::string> location =
        ProtoToIsolatedWebAppLocation(local_data.isolation_data());
    if (!location.has_value()) {
      DLOG(ERROR) << "WebApp proto isolation_data.location" << location.error();
      return nullptr;
    }

    absl::optional<WebApp::IsolationData::PendingUpdateInfo>
        pending_update_info;
    if (local_data.isolation_data().has_pending_update_info()) {
      const auto& pending_update_info_proto =
          local_data.isolation_data().pending_update_info();

      base::expected<IsolatedWebAppLocation, std::string> pending_location =
          ProtoToIsolatedWebAppLocation(pending_update_info_proto);
      if (!pending_location.has_value()) {
        DLOG(ERROR)
            << "WebApp proto isolation_data.pending_update_info.location"
            << pending_location.error();
        return nullptr;
      }
      if (pending_location->index() != location->index()) {
        DLOG(ERROR) << "WebApp proto isolation_data.pending_update_info "
                       "deserialization "
                       "error: isolation_data.pending_update_info.location "
                       "must have the same type as isolation_data.location.";
        return nullptr;
      }

      auto pending_version_components =
          ParseIwaVersionIntoComponents(pending_update_info_proto.version());
      if (!pending_version_components.has_value()) {
        DLOG(ERROR)
            << "WebApp proto isolation_data.pending_update_info.version parse "
               "error: cannot deserialize version: "
            << IwaVersionParseErrorToString(pending_version_components.error());
        return nullptr;
      }
      base::Version pending_version(
          std::vector(pending_version_components->begin(),
                      pending_version_components->end()));

      pending_update_info = WebApp::IsolationData::PendingUpdateInfo(
          *pending_location, pending_version);
    }

    web_app->SetIsolationData(WebApp::IsolationData(
        *location, version, controlled_frame_partitions, pending_update_info));
  }

  return web_app;
}

void WebAppDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&WebAppDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebAppDatabase::OnAllDataRead(
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB data read error: " << error->ToString();
    return;
  }

  store_->ReadAllMetadata(base::BindOnce(
      &WebAppDatabase::OnAllMetadataRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(data_records), std::move(callback)));
}

void WebAppDatabase::OnAllMetadataRead(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB metadata read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::ModelTypeStore::Record& record : *data_records) {
    const AppId app_id = record.id;
    std::unique_ptr<WebApp> web_app = ParseWebApp(app_id, record.value);
    if (web_app)
      registry.emplace(app_id, std::move(web_app));
  }

  opened_ = true;
  // This should be a tail call: a callback code may indirectly call |this|
  // methods, like WebAppDatabase::Write()
  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
}

void WebAppDatabase::OnDataWritten(
    CompletionCallback callback,
    const absl::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();
  }

  std::move(callback).Run(!error);
}

// static
std::unique_ptr<WebApp> WebAppDatabase::ParseWebApp(const AppId& app_id,
                                                    const std::string& value) {
  WebAppProto proto;
  const bool parsed = proto.ParseFromString(value);
  if (!parsed) {
    DLOG(ERROR) << "WebApps LevelDB parse error: can't parse proto.";
    return nullptr;
  }

  auto web_app = CreateWebApp(proto);
  if (!web_app) {
    // CreateWebApp() already logged what went wrong here.
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

DisplayMode ToMojomDisplayMode(WebAppProto::DisplayMode display_mode) {
  switch (display_mode) {
    case WebAppProto::BROWSER:
      return DisplayMode::kBrowser;
    case WebAppProto::MINIMAL_UI:
      return DisplayMode::kMinimalUi;
    case WebAppProto::STANDALONE:
      return DisplayMode::kStandalone;
    case WebAppProto::FULLSCREEN:
      return DisplayMode::kFullscreen;
    case WebAppProto::WINDOW_CONTROLS_OVERLAY:
      return DisplayMode::kWindowControlsOverlay;
    case WebAppProto::TABBED:
      return DisplayMode::kTabbed;
    case WebAppProto::BORDERLESS:
      return DisplayMode::kBorderless;
  }
}

DisplayMode ToMojomDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return DisplayMode::kBrowser;
    case ::sync_pb::WebAppSpecifics::TABBED:
      return DisplayMode::kTabbed;
    // New display modes will most likely be of the window variety than the
    // browser tab variety so default to windowed if it's an enum value we don't
    // know about.
    case ::sync_pb::WebAppSpecifics::UNSPECIFIED:
    case ::sync_pb::WebAppSpecifics::STANDALONE:
      return DisplayMode::kStandalone;
  }
}

WebAppProto::DisplayMode ToWebAppProtoDisplayMode(DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return WebAppProto::BROWSER;
    case DisplayMode::kMinimalUi:
      return WebAppProto::MINIMAL_UI;
    case DisplayMode::kUndefined:
      NOTREACHED();
      [[fallthrough]];
    case DisplayMode::kStandalone:
      return WebAppProto::STANDALONE;
    case DisplayMode::kFullscreen:
      return WebAppProto::FULLSCREEN;
    case DisplayMode::kWindowControlsOverlay:
      return WebAppProto::WINDOW_CONTROLS_OVERLAY;
    case DisplayMode::kTabbed:
      return WebAppProto::TABBED;
    case DisplayMode::kBorderless:
      return WebAppProto::BORDERLESS;
  }
}

}  // namespace web_app
