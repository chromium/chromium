// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_test_util.h"

namespace web_app {

TestWebAppDatabaseFactory::TestWebAppDatabaseFactory() {
  // InMemoryStore must be created after message_loop_.
  store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
}

TestWebAppDatabaseFactory::~TestWebAppDatabaseFactory() {}

syncer::OnceModelTypeStoreFactory TestWebAppDatabaseFactory::GetStoreFactory() {
  return syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
      store_.get());
}

Registry TestWebAppDatabaseFactory::ReadRegistry() const {
  Registry registry;
  base::RunLoop run_loop;

  store_->ReadAllData(base::BindLambdaForTesting(
      [&](const base::Optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
        DCHECK(!error);

        for (const syncer::ModelTypeStore::Record& record : *data_records) {
          auto app = WebAppDatabase::ParseWebApp(record.id, record.value);
          DCHECK(app);

          AppId app_id = app->app_id();
          registry.emplace(std::move(app_id), std::move(app));
        }
        run_loop.Quit();
      }));

  run_loop.Run();
  return registry;
}

std::set<AppId> TestWebAppDatabaseFactory::ReadAllAppIds() const {
  std::set<AppId> app_ids;

  auto registry = ReadRegistry();
  for (auto& kv : registry)
    app_ids.insert(kv.first);

  return app_ids;
}

void TestWebAppDatabaseFactory::WriteRegistry(const Registry& registry) {
  base::RunLoop run_loop;

  auto write_batch = store_->CreateWriteBatch();

  for (auto& kv : registry) {
    const WebApp* app = kv.second.get();
    auto proto = WebAppDatabase::CreateWebAppProto(*app);
    write_batch->WriteData(app->app_id(), proto->SerializeAsString());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(base::BindLambdaForTesting(
          [&](const base::Optional<syncer::ModelError>& error) {
            DCHECK(!error);
            run_loop.Quit();
          })));

  run_loop.Run();
}

}  // namespace web_app
