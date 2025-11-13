// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_database_metadata.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"

namespace syncer {
class ModelError;
class MetadataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebApp;
namespace proto {
class WebApp;
}  // namespace proto
struct RegistryUpdateData;

// Exclusively used from the UI thread.
class WebAppDatabase {
 public:
  using ReportErrorCallback =
      base::RepeatingCallback<void(const syncer::ModelError&)>;

  static constexpr std::string_view kDatabaseMetadataKey = "DATABASE_METADATA";

  WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory,
                 ReportErrorCallback error_callback);
  WebAppDatabase(const WebAppDatabase&) = delete;
  WebAppDatabase& operator=(const WebAppDatabase&) = delete;
  ~WebAppDatabase();

  using RegistryOpenedCallback = base::OnceCallback<void(
      Registry registry,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch)>;
  // Open existing or create new DB. Read all data and return it via callback.
  void OpenDatabase(RegistryOpenedCallback callback);

  using CompletionCallback = base::OnceCallback<void(bool success)>;
  void Write(const RegistryUpdateData& update_data,
             std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
             CompletionCallback callback);

  bool is_opened() const { return opened_; }

  // Returns the version that the database will be migrated to when opened.
  // - No version/version 0 is the original version.
  // - Version 1 introduces the UserInstalled install source, migration between
  //   0 and 1 add or remove this source.
  // - Version 2 migrates shortcut apps to DIY apps, ensures platform user
  //   display mode is set, and fixes partial install state inconsistencies.
  // - Version 3 migrates deprecated launch handler fields to client_mode,
  //   removes query/ref from scope, and ensures relative_manifest_id exists
  //   without fragments.
  static int GetCurrentDatabaseVersion();

 private:
  struct ProtobufState {
    ProtobufState();
    ~ProtobufState();
    ProtobufState(ProtobufState&&);
    ProtobufState& operator=(ProtobufState&&);

    proto::DatabaseMetadata metadata;
    base::flat_map<webapps::AppId, proto::WebApp> apps;
  };

  ProtobufState ParseProtobufs(
      const syncer::DataTypeStore::RecordList& data_records) const;

  void MigrateDatabase(ProtobufState& state);
  void MigrateInstallSourceAddUserInstalled(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);
  // Migrates apps that were created as shortcuts (empty scope or installed via
  // "Create shortcut") to be DIY apps with a valid scope derived from the start
  // URL.
  void MigrateShortcutAppsToDiyApps(ProtobufState& state,
                                    std::set<webapps::AppId>& changed_apps);
  // Ensures that the user display mode is set for the current platform in the
  // sync proto. If it's missing, it derives it from the other platform's
  // setting or defaults to STANDALONE.
  void MigrateDefaultDisplayModeToPlatformDisplayMode(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);
  // Corrects the install_state for apps that claim OS integration but lack the
  // necessary OS integration state data.
  void MigratePartiallyInstalledAppsToCorrectState(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);
  // Migrates deprecated launch handler fields (`route_to`,
  // `navigate_existing_client`) to the `client_mode` field. Also handles the
  // `client_mode_valid_and_specified` field correctly.
  void MigrateDeprecatedLaunchHandlerToClientMode(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);
  // Migrates the `scope` field by removing any query or ref components.
  void MigrateScopeToRemoveRefAndQuery(ProtobufState& state,
                                       std::set<webapps::AppId>& changed_apps);
  // Ensures the `relative_manifest_id` field exists and does not contain URL
  // fragments. Populates it from the start_url if missing. Records a histogram
  // if an existing fragment needed removal.
  void MigrateToRelativeManifestIdNoFragment(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);

  // Ensures that `pending_update_info.was_ignored` field exists and is default
  // initialized to `false`.
  void MigratePendingUpdateInfoWasIgnored(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);

  // Ensures that if the icon metadata for a pending update is corrupted for the
  // web app, then the icon metadata is cleared. A pending update containing
  // icons is considered to be corrupt if either of the following conditions are
  // true:
  // 1. If manifest_icons exist, but the downloaded_manifest_icons is empty (and
  // vice-versa).
  // 2. If trusted_icons exist, but the downloaded_trusted_icons is empty (and
  // vice-versa).
  // 3. If the data in the icons are corrupt, like they have missing urls or
  // purposes.
  // 4. If the data in the downloaded_<X> fields are corrupt, like missing sizes
  // or purposes.
  void MigratePendingUpdateInfoClearIconMetadataIfCorrupted(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);

  void OnDatabaseOpened(RegistryOpenedCallback callback,
                        const std::optional<syncer::ModelError>& error,
                        std::unique_ptr<syncer::DataTypeStore> store);

  void OnAllDataAndMetadataRead(
      RegistryOpenedCallback callback,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataWritten(CompletionCallback callback,
                     const std::optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::DataTypeStore> store_;
  const raw_ptr<AbstractWebAppDatabaseFactory, DanglingUntriaged>
      database_factory_;
  ReportErrorCallback error_callback_;

  // Database is opened if store is created and all data read.
  bool opened_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebAppDatabase> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
