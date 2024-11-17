// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_

#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"

namespace syncer {
class DataTypeStore;
}  // namespace syncer
namespace web_app {

namespace proto {
class DatabaseMetadata;
}  // namespace proto

class WebAppProto;

class FakeWebAppDatabaseFactory : public AbstractWebAppDatabaseFactory {
 public:
  FakeWebAppDatabaseFactory();
  FakeWebAppDatabaseFactory(const FakeWebAppDatabaseFactory&) = delete;
  FakeWebAppDatabaseFactory& operator=(const FakeWebAppDatabaseFactory&) =
      delete;
  ~FakeWebAppDatabaseFactory() override;

  syncer::DataTypeStore* GetStore();

  // AbstractWebAppDatabaseFactory interface implementation.
  syncer::OnceDataTypeStoreFactory GetStoreFactory() override;
  bool IsSyncingApps() override;

  proto::DatabaseMetadata ReadMetadata();
  Registry ReadRegistry();

  std::set<webapps::AppId> ReadAllAppIds();

  void WriteProtos(const std::vector<std::unique_ptr<WebAppProto>>& protos);
  void WriteRegistry(const Registry& registry);

  void set_is_syncing_apps(bool is_syncing_apps) {
    is_syncing_apps_ = is_syncing_apps;
  }

 private:
  std::unique_ptr<syncer::DataTypeStore> store_;
  bool is_syncing_apps_ = true;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_
