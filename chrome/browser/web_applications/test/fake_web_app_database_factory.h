// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_

#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace syncer {
class ModelTypeStore;
}  // namespace syncer

namespace web_app {

class WebAppProto;

class FakeWebAppDatabaseFactory : public AbstractWebAppDatabaseFactory {
 public:
  FakeWebAppDatabaseFactory();
  FakeWebAppDatabaseFactory(const FakeWebAppDatabaseFactory&) = delete;
  FakeWebAppDatabaseFactory& operator=(const FakeWebAppDatabaseFactory&) =
      delete;
  ~FakeWebAppDatabaseFactory() override;

  syncer::ModelTypeStore* GetStore();

  // AbstractWebAppDatabaseFactory interface implementation.
  syncer::OnceModelTypeStoreFactory GetStoreFactory() override;

  Registry ReadRegistry();

  std::set<AppId> ReadAllAppIds();

  void WriteProtos(const std::vector<std::unique_ptr<WebAppProto>>& protos);
  void WriteRegistry(const Registry& registry);

 private:
  std::unique_ptr<syncer::ModelTypeStore> store_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_DATABASE_FACTORY_H_
