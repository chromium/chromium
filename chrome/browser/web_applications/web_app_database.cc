// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_launch_handler.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_database_serialization.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Check if the icon metadata stored for pending updates is corrupt.
bool CorruptIconMetadataForIcons(
    const ::google::protobuf::RepeatedPtrField<::sync_pb::WebAppIconInfo>&
        icons) {
  for (const auto& icon : icons) {
    if (!icon.has_url() || !icon.has_purpose()) {
      return true;
    }
  }
  return false;
}

// Check if the downloaded icon sizes stored for pending updates is corrupt.
bool CorruptDownloadedSizeMetadata(
    const ::google::protobuf::RepeatedPtrField<proto::DownloadedIconSizeInfo>&
        downloaded_icon_sizes) {
  for (const auto& downloaded_icon : downloaded_icon_sizes) {
    // It's fine if there are no sizes specified for a purpose, but the purpose
    // has to exist.
    if (!downloaded_icon.has_purpose()) {
      return true;
    }
  }
  return false;
}

}  // namespace

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

  syncer::OnceDataTypeStoreFactory store_factory =
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

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // |update_data| can be empty here but we should write |metadata_change_list|
  // anyway.
  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    auto proto = WebAppToProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    auto proto = WebAppToProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const webapps::AppId& app_id : update_data.apps_to_delete) {
    write_batch->DeleteData(app_id);
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&WebAppDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
int WebAppDatabase::GetCurrentDatabaseVersion() {
  return 5;
}

WebAppDatabase::ProtobufState::ProtobufState() = default;
WebAppDatabase::ProtobufState::~ProtobufState() = default;
WebAppDatabase::ProtobufState::ProtobufState(ProtobufState&&) = default;
WebAppDatabase::ProtobufState& WebAppDatabase::ProtobufState::operator=(
    ProtobufState&&) = default;

WebAppDatabase::ProtobufState WebAppDatabase::ParseProtobufs(
    const syncer::DataTypeStore::RecordList& data_records) const {
  ProtobufState state;
  for (const syncer::DataTypeStore::Record& record : data_records) {
    if (record.id == kDatabaseMetadataKey) {
      bool success = state.metadata.ParseFromString(record.value);
      if (!success) {
        DLOG(ERROR)
            << "WebApps LevelDB parse error: can't parse metadata proto.";
        // TODO: Consider logging a histogram
      }
      continue;
    }

    proto::WebApp app_proto;
    bool success = app_proto.ParseFromString(record.value);
    if (!success) {
      DLOG(ERROR) << "WebApps LevelDB parse error: can't parse app proto.";
      // TODO: Consider logging a histogram
    }
    state.apps.emplace(record.id, std::move(app_proto));
  }
  return state;
}

void WebAppDatabase::MigrateDatabase(ProtobufState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Migration should happen when we have gotten a `store_`, but haven't
  // finished opening the database yet.
  CHECK(store_);
  CHECK(!opened_);

  bool did_change_metadata = false;
  std::set<webapps::AppId> changed_apps;

  // Upgrade from version 0 to version 1. This migrates the kSync source to
  // a combination of kSync and kUserInstalled.
  if (state.metadata.version() < 1 && GetCurrentDatabaseVersion() >= 1) {
    MigrateInstallSourceAddUserInstalled(state, changed_apps);
    base::UmaHistogramSparse("WebApp.Database.VersionUpgradedTo", 1);
    state.metadata.set_version(1);
    did_change_metadata = true;
  }

  // Upgrade from version 1 to version 2.
  if (state.metadata.version() < 2 && GetCurrentDatabaseVersion() >= 2) {
    MigrateShortcutAppsToDiyApps(state, changed_apps);
    MigrateDefaultDisplayModeToPlatformDisplayMode(state, changed_apps);
    MigratePartiallyInstalledAppsToCorrectState(state, changed_apps);
    base::UmaHistogramSparse("WebApp.Database.VersionUpgradedTo", 2);
    state.metadata.set_version(2);
    did_change_metadata = true;
  }

  // Upgrade from version 2 to version 3.
  if (state.metadata.version() < 3 && GetCurrentDatabaseVersion() >= 3) {
    MigrateDeprecatedLaunchHandlerToClientMode(state, changed_apps);
    MigrateScopeToRemoveRefAndQuery(state, changed_apps);
    MigrateToRelativeManifestIdNoFragment(state, changed_apps);
    base::UmaHistogramSparse("WebApp.Database.VersionUpgradedTo", 3);
    state.metadata.set_version(3);
    did_change_metadata = true;
  }

  // Upgrade from version 3 to version 4.
  if (state.metadata.version() < 4 && GetCurrentDatabaseVersion() >= 4) {
    MigratePendingUpdateInfoWasIgnored(state, changed_apps);
    base::UmaHistogramSparse("WebApp.Database.VersionUpgradedTo", 4);
    state.metadata.set_version(4);
    did_change_metadata = true;
  }

  // Upgrade from version 4 to version 5.
  if (state.metadata.version() < 5 && GetCurrentDatabaseVersion() >= 5) {
    MigratePendingUpdateInfoClearIconMetadataIfCorrupted(state, changed_apps);
    base::UmaHistogramSparse("WebApp.Database.VersionUpgradedTo", 5);
    state.metadata.set_version(5);
    did_change_metadata = true;
  }

  CHECK_EQ(state.metadata.version(), GetCurrentDatabaseVersion());

  if (did_change_metadata || !changed_apps.empty()) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        store_->CreateWriteBatch();
    if (did_change_metadata) {
      write_batch->WriteData(std::string(kDatabaseMetadataKey),
                             state.metadata.SerializeAsString());
    }
    for (const auto& app_id : changed_apps) {
      CHECK(state.apps.contains(app_id));
      write_batch->WriteData(app_id, state.apps[app_id].SerializeAsString());
    }
    store_->CommitWriteBatch(
        std::move(write_batch),
        base::BindOnce(&WebAppDatabase::OnDataWritten,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
  }
}

void WebAppDatabase::MigrateInstallSourceAddUserInstalled(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 0 to version 1.
  CHECK_LT(state.metadata.version(), 1);
  const bool is_syncing_apps = database_factory_->IsSyncingApps();
  int apps_migrated_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (!app_proto.sources().sync()) {
      continue;
    }
    bool changed = false;
    if (!app_proto.sources().user_installed()) {
      app_proto.mutable_sources()->set_user_installed(true);
      changed = true;
    }
    if (!is_syncing_apps) {
      app_proto.mutable_sources()->set_sync(false);
      changed = true;
    }
    if (changed) {
      changed_apps.insert(app_id);
      apps_migrated_count++;
    }
  }
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.InstallSourceAddUserInstalled", apps_migrated_count);
}

void WebAppDatabase::MigrateShortcutAppsToDiyApps(
    WebAppDatabase::ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 1 to version 2.
  CHECK_LT(state.metadata.version(), 2);
  int shortcut_to_diy_apps = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    bool is_shortcut =
        !app_proto.has_scope() || app_proto.scope().empty() ||
        (app_proto.has_latest_install_source() &&
         app_proto.latest_install_source() ==
             static_cast<uint32_t>(
                 webapps::WebappInstallSource::MENU_CREATE_SHORTCUT));
    if (!is_shortcut) {
      continue;
    }
    changed_apps.insert(app_id);
    app_proto.set_is_diy_app(true);
    app_proto.set_was_shortcut_app(true);
    shortcut_to_diy_apps++;
    if (app_proto.has_scope() && !app_proto.scope().empty() &&
        GURL(app_proto.scope()).is_valid()) {
      continue;
    }
    // Populate the scope if it was empty or invalid.
    if (!app_proto.has_sync_data() || !app_proto.sync_data().has_start_url()) {
      DLOG(ERROR) << "Missing sync data or start_url for shortcut app "
                  << app_id;
      continue;
    }
    GURL start_url(app_proto.sync_data().start_url());
    if (!start_url.is_valid()) {
      // Cannot recover scope, mark for potential cleanup later if needed.
      DLOG(ERROR) << "Invalid start_url for shortcut app " << app_id << ":"
                  << start_url.possibly_invalid_spec();
      continue;
    }
    app_proto.set_scope(start_url.GetWithoutFilename().spec());
  }
  base::UmaHistogramCounts1000("WebApp.Migrations.ShortcutAppsToDiy2",
                               shortcut_to_diy_apps);
}

void WebAppDatabase::MigrateDefaultDisplayModeToPlatformDisplayMode(
    WebAppDatabase::ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 1 to version 2.
  CHECK_LT(state.metadata.version(), 2);
  int apps_migrated_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (!app_proto.has_sync_data()) {
      // Cannot migrate without sync data.
      continue;
    }
    sync_pb::WebAppSpecifics* sync_data = app_proto.mutable_sync_data();
    if (!HasCurrentPlatformUserDisplayMode(*sync_data)) {
      sync_pb::WebAppSpecifics_UserDisplayMode udm =
          ResolvePlatformSpecificUserDisplayMode(*sync_data);
      SetPlatformSpecificUserDisplayMode(udm, sync_data);
      changed_apps.insert(app_id);
      apps_migrated_count++;
    }
  }
  base::UmaHistogramCounts1000("WebApp.Migrations.DefaultDisplayModeToPlatform",
                               apps_migrated_count);
}

// Corrects the install_state for apps that claim OS integration but lack the
// necessary OS integration state data.
void WebAppDatabase::MigratePartiallyInstalledAppsToCorrectState(
    WebAppDatabase::ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 1 to version 2.
  CHECK_LT(state.metadata.version(), 2);
  int install_state_fixed_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (app_proto.install_state() !=
        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
      continue;
    }
    // Check if any OS integration state exists. A simple check for shortcut
    // presence is sufficient as a proxy for any OS integration.
    if (app_proto.has_current_os_integration_states() &&
        app_proto.current_os_integration_states().has_shortcut()) {
      continue;
    }
    app_proto.set_install_state(
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
    changed_apps.insert(app_id);
    install_state_fixed_count++;
  }
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.PartiallyInstalledAppsToCorrectState",
      install_state_fixed_count);
}

void WebAppDatabase::MigrateDeprecatedLaunchHandlerToClientMode(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 2 to version 3.
  CHECK_LT(state.metadata.version(), 3);
  int apps_migrated_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (!app_proto.has_launch_handler()) {
      continue;
    }

    bool changed = false;
    proto::LaunchHandler* launch_handler = app_proto.mutable_launch_handler();

    // If client_mode is unspecified, try migrating from deprecated fields.
    if (launch_handler->client_mode() ==
        proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED) {
      proto::LaunchHandler::ClientMode migrated_client_mode =
          proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED;
      switch (launch_handler->route_to()) {
        case proto::LaunchHandler_DeprecatedRouteTo_UNSPECIFIED_ROUTE:
          break;
        case proto::LaunchHandler_DeprecatedRouteTo_AUTO_ROUTE:
          migrated_client_mode = proto::LaunchHandler::CLIENT_MODE_AUTO;
          break;
        case proto::LaunchHandler_DeprecatedRouteTo_NEW_CLIENT:
          migrated_client_mode = proto::LaunchHandler::CLIENT_MODE_NAVIGATE_NEW;
          break;
        case proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT:
          if (launch_handler->navigate_existing_client() ==
              proto::LaunchHandler_DeprecatedNavigateExistingClient_NEVER) {
            migrated_client_mode =
                proto::LaunchHandler::CLIENT_MODE_FOCUS_EXISTING;
          } else {
            migrated_client_mode =
                proto::LaunchHandler::CLIENT_MODE_NAVIGATE_EXISTING;
          }
          break;
        case proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT_NAVIGATE:
          migrated_client_mode =
              proto::LaunchHandler::CLIENT_MODE_NAVIGATE_EXISTING;
          break;
        case proto::LaunchHandler_DeprecatedRouteTo_EXISTING_CLIENT_RETAIN:
          migrated_client_mode =
              proto::LaunchHandler::CLIENT_MODE_FOCUS_EXISTING;
          break;
      }
      launch_handler->set_client_mode(migrated_client_mode);
      changed = true;
    } else if (launch_handler->client_mode() ==
               proto::LaunchHandler::CLIENT_MODE_AUTO) {
      // If client_mode is set to auto, and client_mode_valid_and_specified is
      // explicitly false, treat client_mode as unspecified.
      if (launch_handler->has_client_mode_valid_and_specified() &&
          !launch_handler->client_mode_valid_and_specified()) {
        launch_handler->set_client_mode(
            proto::LaunchHandler::CLIENT_MODE_UNSPECIFIED);
        changed = true;
      }
    }

    // Clear deprecated fields if they exist.
    if (launch_handler->has_route_to()) {
      launch_handler->clear_route_to();
      changed = true;
    }
    if (launch_handler->has_navigate_existing_client()) {
      launch_handler->clear_navigate_existing_client();
      changed = true;
    }
    if (launch_handler->has_client_mode_valid_and_specified()) {
      launch_handler->clear_client_mode_valid_and_specified();
      changed = true;
    }

    if (changed) {
      changed_apps.insert(app_id);
      apps_migrated_count++;
    }
  }
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.DeprecatedLaunchHandlerToClientMode",
      apps_migrated_count);
}

void WebAppDatabase::MigrateScopeToRemoveRefAndQuery(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 2 to version 3.
  CHECK_LT(state.metadata.version(), 3);
  int apps_migrated_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (!app_proto.has_scope()) {
      continue;
    }
    GURL scope(app_proto.scope());
    if (!scope.is_valid()) {
      continue;
    }

    if (scope.has_query() || scope.has_ref()) {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      replacements.ClearRef();
      GURL clean_scope = scope.ReplaceComponents(replacements);
      app_proto.set_scope(clean_scope.spec());
      changed_apps.insert(app_id);
      apps_migrated_count++;
    }
  }
  base::UmaHistogramCounts1000("WebApp.Migrations.ScopeRefQueryRemoved",
                               apps_migrated_count);
}

void WebAppDatabase::MigrateToRelativeManifestIdNoFragment(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 2 to version 3.
  CHECK_LT(state.metadata.version(), 3);
  int apps_migrated_count = 0;
  int fragment_removed_count = 0;
  for (auto& [app_id, app_proto] : state.apps) {
    if (!app_proto.has_sync_data()) {
      continue;
    }
    sync_pb::WebAppSpecifics* sync_data = app_proto.mutable_sync_data();
    if (!sync_data->has_start_url()) {
      continue;
    }
    GURL start_url(sync_data->start_url());
    if (!start_url.is_valid()) {
      continue;
    }

    // Calculate the expected manifest_id and relative path without fragment.
    webapps::ManifestId expected_manifest_id;
    if (sync_data->has_relative_manifest_id()) {
      expected_manifest_id =
          GenerateManifestId(sync_data->relative_manifest_id(), start_url);
    } else {
      expected_manifest_id = GenerateManifestIdFromStartUrlOnly(start_url);
    }
    if (!expected_manifest_id.is_valid()) {
      continue;
    }
    std::string expected_relative_path =
        RelativeManifestIdPath(expected_manifest_id);

    bool changed = false;
    if (!sync_data->has_relative_manifest_id()) {
      // Populate if missing.
      sync_data->set_relative_manifest_id(expected_relative_path);
      changed = true;
    } else if (sync_data->relative_manifest_id() != expected_relative_path) {
      // Correct if different (e.g., had a fragment).
      sync_data->set_relative_manifest_id(expected_relative_path);
      changed = true;
      fragment_removed_count++;
    }

    if (changed) {
      changed_apps.insert(app_id);
      apps_migrated_count++;
    }
  }
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.RelativeManifestIdFragmentRemoved",
      fragment_removed_count);
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.RelativeManifestIdPopulatedOrFixed",
      apps_migrated_count);
}

void WebAppDatabase::MigratePendingUpdateInfoWasIgnored(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 3 to version 4.
  CHECK_LT(state.metadata.version(), 4);
  int apps_migrated_count = 0;

  for (auto& [app_id, app_proto] : state.apps) {
    // Bypass apps that don't have a pending update info, or has the
    // `was_ignored` field set.
    if (!app_proto.has_pending_update_info() ||
        app_proto.pending_update_info().has_was_ignored()) {
      continue;
    }

    apps_migrated_count++;
    app_proto.mutable_pending_update_info()->set_was_ignored(false);
    changed_apps.insert(app_id);
  }
  // Record histograms correctly.
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.PendingInfoWasIgnoredMigrated", apps_migrated_count);
}

void WebAppDatabase::MigratePendingUpdateInfoClearIconMetadataIfCorrupted(
    ProtobufState& state,
    std::set<webapps::AppId>& changed_apps) {
  // Migrating from version 4 to version 5.
  CHECK_LT(state.metadata.version(), 5);
  int corrupted_apps_count = 0;

  for (auto& [app_id, app_proto] : state.apps) {
    // Bypass apps that don't have a pending update info.
    if (!app_proto.has_pending_update_info()) {
      continue;
    }

    bool manifest_icons_corrupted =
        (app_proto.pending_update_info().manifest_icons().empty() !=
             app_proto.pending_update_info()
                 .downloaded_manifest_icons()
                 .empty() ||
         CorruptIconMetadataForIcons(
             app_proto.pending_update_info().manifest_icons()) ||
         CorruptDownloadedSizeMetadata(
             app_proto.pending_update_info().downloaded_manifest_icons()));
    bool trusted_icons_corrupted =
        (app_proto.pending_update_info().trusted_icons().empty() !=
             app_proto.pending_update_info()
                 .downloaded_trusted_icons()
                 .empty() ||
         CorruptIconMetadataForIcons(
             app_proto.pending_update_info().trusted_icons()) ||
         CorruptDownloadedSizeMetadata(
             app_proto.pending_update_info().downloaded_trusted_icons()));
    if (!manifest_icons_corrupted && !trusted_icons_corrupted) {
      continue;
    }

    // At this point, icon metadata is corrupted. Clear them to prevent the
    // proto web app serialization logic from dropping them.
    corrupted_apps_count++;
    app_proto.mutable_pending_update_info()->clear_manifest_icons();
    app_proto.mutable_pending_update_info()->clear_trusted_icons();
    app_proto.mutable_pending_update_info()->clear_downloaded_manifest_icons();
    app_proto.mutable_pending_update_info()->clear_downloaded_trusted_icons();

    // If this was just an icon update, then having an empty PendingUpdateInfo
    // with no pending update metadata does not make sense. Clear the whole
    // field in that case.
    if (!app_proto.pending_update_info().has_name()) {
      app_proto.clear_pending_update_info();
    }
    changed_apps.insert(app_id);
  }
  base::UmaHistogramCounts1000(
      "WebApp.Migrations.PendingUpdateInfoIconDataCorrupted",
      corrupted_apps_count);
}

void WebAppDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&WebAppDatabase::OnAllDataAndMetadataRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppDatabase::OnAllDataAndMetadataRead(
    RegistryOpenedCallback callback,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "WebAppDatabase::OnAllMetadataRead");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB read error: " << error->ToString();
    return;
  }

  ProtobufState state = ParseProtobufs(*data_records);
  MigrateDatabase(state);

  Registry registry;
  for (const auto& [app_id, app_proto] : state.apps) {
    std::unique_ptr<WebApp> web_app = ParseWebAppProto(app_proto);
    base::UmaHistogramBoolean("WebApp.Database.ValidProto", web_app != nullptr);
    if (!web_app) {
      continue;
    }

    // Record whether the derived app_id matches the database key.
    bool mismatch = (web_app->app_id() != app_id);
    base::UmaHistogramBoolean("WebApp.Database.AppIdMatch", !mismatch);

    if (mismatch) {
      DLOG(ERROR) << "WebApps LevelDB error: app_id doesn't match storage key "
                  << app_id << " vs " << web_app->app_id() << ", from "
                  << web_app->manifest_id();
      continue;
    }
    registry.emplace(app_id, std::move(web_app));
  }

  opened_ = true;
  // This should be a tail call: a callback code may indirectly call |this|
  // methods, like WebAppDatabase::Write()
  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
}

void WebAppDatabase::OnDataWritten(
    CompletionCallback callback,
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();
  }

  std::move(callback).Run(!error);
}

}  // namespace web_app
