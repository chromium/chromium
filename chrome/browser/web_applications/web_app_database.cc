// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
      FALLTHROUGH;
    case blink::mojom::CaptureLinks::kNone:
      return WebAppProto_CaptureLinks_NONE;
    case blink::mojom::CaptureLinks::kNewClient:
      return WebAppProto_CaptureLinks_NEW_CLIENT;
    case blink::mojom::CaptureLinks::kExistingClientNavigate:
      return WebAppProto_CaptureLinks_EXISTING_CLIENT_NAVIGATE;
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
  DCHECK(!start_url.is_empty() && start_url.is_valid());

  DCHECK(!web_app.app_id().empty());

  // Set sync data to sync proto.
  *(local_data->mutable_sync_data()) = WebAppToSyncProto(web_app);

  local_data->set_name(web_app.name());

  DCHECK(web_app.sources_.any());
  local_data->mutable_sources()->set_system(web_app.sources_[Source::kSystem]);
  local_data->mutable_sources()->set_policy(web_app.sources_[Source::kPolicy]);
  local_data->mutable_sources()->set_web_app_store(
      web_app.sources_[Source::kWebAppStore]);
  local_data->mutable_sources()->set_sync(web_app.sources_[Source::kSync]);
  local_data->mutable_sources()->set_default_(
      web_app.sources_[Source::kDefault]);

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

  local_data->set_description(web_app.description());
  if (!web_app.scope().is_empty())
    local_data->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    local_data->set_theme_color(web_app.theme_color().value());
  if (web_app.background_color().has_value())
    local_data->set_background_color(web_app.background_color().value());
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

  if (web_app.chromeos_data().has_value()) {
    auto& chromeos_data = web_app.chromeos_data().value();
    auto* mutable_chromeos_data = local_data->mutable_chromeos_data();
    mutable_chromeos_data->set_show_in_launcher(chromeos_data.show_in_launcher);
    mutable_chromeos_data->set_show_in_search(chromeos_data.show_in_search);
    mutable_chromeos_data->set_show_in_management(
        chromeos_data.show_in_management);
    mutable_chromeos_data->set_is_disabled(chromeos_data.is_disabled);
    mutable_chromeos_data->set_oem_installed(chromeos_data.oem_installed);
  }

  if (web_app.client_data().system_web_app_data.has_value()) {
    auto& swa_data = web_app.client_data().system_web_app_data.value();

    auto* mutable_swa_data =
        local_data->mutable_client_data()->mutable_system_web_app_data();
    mutable_swa_data->set_system_app_type(
        static_cast<SystemWebAppDataProto_SystemAppType>(
            swa_data.system_app_type));
  }

  local_data->set_user_run_on_os_login_mode(
      ToWebAppProtoRunOnOsLoginMode(web_app.run_on_os_login_mode()));
  local_data->set_is_in_sync_install(web_app.is_in_sync_install());

  for (const WebApplicationIconInfo& icon_info : web_app.icon_infos())
    *(local_data->add_icon_infos()) = WebAppIconInfoToSyncProto(icon_info);

  for (SquareSizePx size : web_app.downloaded_icon_sizes(IconPurpose::ANY)) {
    local_data->add_downloaded_icon_sizes_purpose_any(size);
  }
  for (SquareSizePx size :
       web_app.downloaded_icon_sizes(IconPurpose::MASKABLE)) {
    local_data->add_downloaded_icon_sizes_purpose_maskable(size);
  }

  local_data->set_is_generated_icon(web_app.is_generated_icon());

  for (const auto& file_handler : web_app.file_handlers()) {
    WebAppFileHandlerProto* file_handler_proto =
        local_data->add_file_handlers();
    file_handler_proto->set_action(file_handler.action.spec());

    for (const auto& accept_entry : file_handler.accept) {
      WebAppFileHandlerAcceptProto* accept_entry_proto =
          file_handler_proto->add_accept();
      accept_entry_proto->set_mimetype(accept_entry.mime_type);

      for (const auto& file_extension : accept_entry.file_extensions)
        accept_entry_proto->add_file_extensions(file_extension);
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

  // TODO (crbug.com/1114638): Support Monochrome icons.
  std::array<IconPurpose, 2> purposes = {IconPurpose::ANY,
                                         IconPurpose::MASKABLE};
  for (const WebApplicationShortcutsMenuItemInfo& shortcut_info :
       web_app.shortcuts_menu_item_infos()) {
    WebAppShortcutsMenuItemInfoProto* shortcut_info_proto =
        local_data->add_shortcuts_menu_item_infos();
    shortcut_info_proto->set_name(base::UTF16ToUTF8(shortcut_info.name));
    shortcut_info_proto->set_url(shortcut_info.url.spec());
    for (IconPurpose purpose : purposes) {
      for (const WebApplicationShortcutsMenuItemInfo::Icon& icon_info :
           shortcut_info.GetShortcutIconInfosForPurpose(purpose)) {
        // TODO (crbug.com/1114638): Add Monochrome support.
        sync_pb::WebAppIconInfo* shortcut_icon_info_proto =
            (purpose == IconPurpose::ANY)
                ? shortcut_info_proto->add_shortcut_icon_infos()
                : shortcut_info_proto->add_shortcut_icon_infos_maskable();
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
    // TODO (crbug.com/1114638): Add Monochrome support.
    for (const SquareSizePx& icon_size :
         icon_sizes.GetSizesForPurpose(IconPurpose::ANY)) {
      icon_sizes_proto->add_icon_sizes(icon_size);
    }
    for (const SquareSizePx& icon_size :
         icon_sizes.GetSizesForPurpose(IconPurpose::MASKABLE)) {
      icon_sizes_proto->add_icon_sizes_maskable(icon_size);
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

  for (const auto& url_handler : web_app.url_handlers()) {
    WebAppUrlHandlerProto* url_handler_proto = local_data->add_url_handlers();
    url_handler_proto->set_origin(url_handler.origin.Serialize());
    url_handler_proto->set_has_origin_wildcard(url_handler.has_origin_wildcard);
  }

  if (web_app.capture_links() != blink::mojom::CaptureLinks::kUndefined)
    local_data->set_capture_links(CaptureLinksToProto(web_app.capture_links()));
  else
    local_data->clear_capture_links();

  if (!web_app.manifest_url().is_empty())
    local_data->set_manifest_url(web_app.manifest_url().spec());

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

  base::Optional<std::string> manifest_id = base::nullopt;
  if (sync_data.has_manifest_id())
    manifest_id = base::Optional<std::string>(sync_data.manifest_id());

  const AppId app_id = GenerateAppId(manifest_id, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);

  web_app->SetManifestId(manifest_id);

  // Required fields:
  if (!local_data.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    return nullptr;
  }

  WebApp::Sources sources;
  sources[Source::kSystem] = local_data.sources().system();
  sources[Source::kPolicy] = local_data.sources().policy();
  sources[Source::kWebAppStore] = local_data.sources().web_app_store();
  sources[Source::kSync] = local_data.sources().sync();
  sources[Source::kDefault] = local_data.sources().default_();
  if (!sources.any()) {
    DLOG(ERROR) << "WebApp proto parse error: no any source in sources field";
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
      ToMojomDisplayMode(sync_data.user_display_mode()));

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

  if (IsChromeOs() && !local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no chromeos_data field. The web "
                << "app might have been installed when running on an OS other "
                << "than Chrome OS.";
    return nullptr;
  }

  if (!IsChromeOs() && local_data.has_chromeos_data()) {
    DLOG(ERROR) << "WebApp proto parse error: has chromeos_data field. The web "
                << "app might have been installed when running on Chrome OS.";
    return nullptr;
  }

  if (local_data.has_chromeos_data()) {
    auto chromeos_data = base::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = chromeos_data_proto.show_in_launcher();
    chromeos_data->show_in_search = chromeos_data_proto.show_in_search();
    chromeos_data->show_in_management =
        chromeos_data_proto.show_in_management();
    chromeos_data->is_disabled = chromeos_data_proto.is_disabled();
    chromeos_data->oem_installed = chromeos_data_proto.oem_installed();
    web_app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  if (local_data.client_data().has_system_web_app_data()) {
    WebAppSystemWebAppData& swa_data =
        web_app->client_data()->system_web_app_data.emplace();

    swa_data.system_app_type = static_cast<SystemAppType>(
        local_data.client_data().system_web_app_data().system_app_type());
  }

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

  if (local_data.has_theme_color())
    web_app->SetThemeColor(local_data.theme_color());

  if (local_data.has_background_color())
    web_app->SetBackgroundColor(local_data.background_color());

  if (local_data.has_is_in_sync_install())
    web_app->SetIsInSyncInstall(local_data.is_in_sync_install());

  if (local_data.has_last_badging_time()) {
    web_app->SetLastBadgingTime(
        syncer::ProtoTimeToTime(local_data.last_badging_time()));
  }
  if (local_data.has_last_launch_time()) {
    web_app->SetLastLaunchTime(
        syncer::ProtoTimeToTime(local_data.last_launch_time()));
  }
  if (local_data.has_install_time()) {
    web_app->SetInstallTime(syncer::ProtoTimeToTime(local_data.install_time()));
  }

  base::Optional<WebApp::SyncFallbackData> parsed_sync_fallback_data =
      ParseSyncFallbackDataStruct(sync_data);
  if (!parsed_sync_fallback_data.has_value()) {
    // ParseSyncFallbackDataStruct() reports any errors.
    return nullptr;
  }
  web_app->SetSyncFallbackData(std::move(parsed_sync_fallback_data.value()));

  base::Optional<std::vector<WebApplicationIconInfo>> parsed_icon_infos =
      ParseWebAppIconInfos("WebApp", local_data.icon_infos());
  if (!parsed_icon_infos.has_value()) {
    // ParseWebAppIconInfos() reports any errors.
    return nullptr;
  }
  web_app->SetIconInfos(std::move(parsed_icon_infos.value()));

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

  web_app->SetIsGeneratedIcon(local_data.is_generated_icon());

  apps::FileHandlers file_handlers;
  for (const auto& file_handler_proto : local_data.file_handlers()) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(file_handler_proto.action());

    if (file_handler.action.is_empty() || !file_handler.action.is_valid()) {
      DLOG(ERROR) << "WebApp FileHandler proto action parse error";
      return nullptr;
    }

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

  std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos;
  // TODO (crbug.com/1114638): Support Monochrome icons.
  std::array<IconPurpose, 2> purposes = {IconPurpose::ANY,
                                         IconPurpose::MASKABLE};
  for (const auto& shortcut_info_proto :
       local_data.shortcuts_menu_item_infos()) {
    WebApplicationShortcutsMenuItemInfo shortcut_info;
    shortcut_info.name = base::UTF8ToUTF16(shortcut_info_proto.name());
    shortcut_info.url = GURL(shortcut_info_proto.url());
    for (IconPurpose purpose : purposes) {
      // TODO (crbug.com/1114638): Add Monochrome support.
      const auto& shortcut_icon_infos =
          (purpose == IconPurpose::ANY)
              ? shortcut_info_proto.shortcut_icon_infos()
              : shortcut_info_proto.shortcut_icon_infos_maskable();
      std::vector<WebApplicationShortcutsMenuItemInfo::Icon> icon_infos;
      for (const auto& icon_info_proto : shortcut_icon_infos) {
        WebApplicationShortcutsMenuItemInfo::Icon shortcut_icon_info;
        shortcut_icon_info.square_size_px = icon_info_proto.size_in_px();
        shortcut_icon_info.url = GURL(icon_info_proto.url());
        icon_infos.emplace_back(std::move(shortcut_icon_info));
      }
      shortcut_info.SetShortcutIconInfosForPurpose(purpose,
                                                   std::move(icon_infos));
    }
    shortcuts_menu_item_infos.emplace_back(std::move(shortcut_info));
  }
  web_app->SetShortcutsMenuItemInfos(std::move(shortcuts_menu_item_infos));

  std::vector<IconSizes> shortcuts_menu_icons_sizes;
  for (const auto& shortcuts_icon_sizes_proto :
       local_data.downloaded_shortcuts_menu_icons_sizes()) {
    IconSizes icon_sizes;
    // TODO (crbug.com/1114638): Support Monochrome icons.
    icon_sizes.SetSizesForPurpose(
        IconPurpose::ANY, std::vector<SquareSizePx>(
                              shortcuts_icon_sizes_proto.icon_sizes().begin(),
                              shortcuts_icon_sizes_proto.icon_sizes().end()));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MASKABLE,
        std::vector<SquareSizePx>(
            shortcuts_icon_sizes_proto.icon_sizes_maskable().begin(),
            shortcuts_icon_sizes_proto.icon_sizes_maskable().end()));

    shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
  }
  web_app->SetDownloadedShortcutsMenuIconsSizes(
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

  if (local_data.has_user_run_on_os_login_mode()) {
    web_app->SetRunOnOsLoginMode(
        ToRunOnOsLoginMode(local_data.user_run_on_os_login_mode()));
  }

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
  return web_app;
}

void WebAppDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  // TODO(loyso): Use ReadAllDataAndPreprocess to parse protos in the background
  // sequence.
  store_->ReadAllData(base::BindOnce(&WebAppDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebAppDatabase::OnAllDataRead(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
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
    const base::Optional<syncer::ModelError>& error,
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
    const base::Optional<syncer::ModelError>& error) {
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
    DLOG(ERROR) << "WebApps LevelDB error: app_id doesn't match storage key";
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
  }
}

DisplayMode ToMojomDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return DisplayMode::kBrowser;
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
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      return WebAppProto::STANDALONE;
    case DisplayMode::kFullscreen:
      return WebAppProto::FULLSCREEN;
    case DisplayMode::kWindowControlsOverlay:
      return WebAppProto::WINDOW_CONTROLS_OVERLAY;
  }
}

}  // namespace web_app
