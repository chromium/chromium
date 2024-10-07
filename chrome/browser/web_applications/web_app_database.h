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
class WebAppProto;
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

  // Exposed for testing.
  static std::unique_ptr<WebAppProto> CreateWebAppProto(const WebApp& web_app);
  // Exposed for testing.
  static std::unique_ptr<WebApp> ParseWebApp(const webapps::AppId& app_id,
                                             const std::string& value);
  // Exposed for testing.
  static std::unique_ptr<WebApp> CreateWebApp(const WebAppProto& local_data);

  bool is_opened() const { return opened_; }

  // Returns the version that the database will be migrated to when opened.
  // - No version/version 0 is the original version.
  // - Version 1 introduces the UserInstalled install source, migration between
  //   0 and 1 add or remove this source.
  static int GetCurrentDatabaseVersion();

 private:
  struct ProtobufState {
    ProtobufState();
    ~ProtobufState();
    ProtobufState(ProtobufState&&);
    ProtobufState& operator=(ProtobufState&&);

    proto::DatabaseMetadata metadata;
    base::flat_map<webapps::AppId, WebAppProto> apps;
  };

  ProtobufState ParseProtobufs(
      const syncer::DataTypeStore::RecordList& data_records) const;

  void MigrateDatabase(ProtobufState& state);
  void MigrateInstallSourceAddUserInstalled(
      ProtobufState& state,
      std::set<webapps::AppId>& changed_apps);
  void MigrateInstallSourceRemoveUserInstalled(
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

DisplayMode ToMojomDisplayMode(WebAppProto::DisplayMode display_mode);

WebAppProto::DisplayMode ToWebAppProtoDisplayMode(DisplayMode display_mode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
