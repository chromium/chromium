// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

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

  WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory,
                 ReportErrorCallback error_callback);
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
  static std::unique_ptr<WebApp> ParseWebApp(const AppId& app_id,
                                             const std::string& value);

 private:
  static std::unique_ptr<WebApp> CreateWebApp(const WebAppProto& local_data);

  void OnDatabaseOpened(RegistryOpenedCallback callback,
                        const base::Optional<syncer::ModelError>& error,
                        std::unique_ptr<syncer::ModelTypeStore> store);

  void OnAllDataRead(
      RegistryOpenedCallback callback,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);
  void OnAllMetadataRead(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
      RegistryOpenedCallback callback,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataWritten(CompletionCallback callback,
                     const base::Optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::ModelTypeStore> store_;
  AbstractWebAppDatabaseFactory* const database_factory_;
  ReportErrorCallback error_callback_;

  // Database is opened if store is created and all data read.
  bool opened_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebAppDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppDatabase);
};

DisplayMode ToMojomDisplayMode(WebAppProto::DisplayMode display_mode);

DisplayMode ToMojomDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode);

WebAppProto::DisplayMode ToWebAppProtoDisplayMode(DisplayMode display_mode);

::sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
    DisplayMode user_display_mode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
