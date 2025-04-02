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
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_database_serialization.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

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
    return 1;
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
  if (state.metadata.version() == 0 && GetCurrentDatabaseVersion() >= 1) {
    MigrateInstallSourceAddUserInstalled(state, changed_apps);
    state.metadata.set_version(1);
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
  for (auto& [app_id, app_proto] : state.apps) {
    if (app_proto.sources().sync()) {
      app_proto.mutable_sources()->set_user_installed(true);
      if (!is_syncing_apps) {
        app_proto.mutable_sources()->set_sync(false);
      }
      changed_apps.insert(app_id);
    }
  }
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
    if (!web_app) {
      continue;
    }

    if (web_app->app_id() != app_id) {
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
