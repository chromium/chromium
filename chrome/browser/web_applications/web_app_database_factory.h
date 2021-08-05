// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_

#include <memory>

#include "components/sync/model/model_type_store.h"

class Profile;

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

namespace web_app {

class AbstractWebAppDatabaseFactory {
 public:
  virtual ~AbstractWebAppDatabaseFactory() = default;
  virtual syncer::OnceModelTypeStoreFactory GetStoreFactory() = 0;
};

class WebAppDatabaseFactory : public AbstractWebAppDatabaseFactory {
 public:
  explicit WebAppDatabaseFactory(Profile* profile);
  WebAppDatabaseFactory(const WebAppDatabaseFactory&) = delete;
  WebAppDatabaseFactory& operator=(const WebAppDatabaseFactory&) = delete;
  ~WebAppDatabaseFactory() override;

  // AbstractWebAppDatabaseFactory implementation.
  syncer::OnceModelTypeStoreFactory GetStoreFactory() override;

 private:
  // If null, the Web Apps system uses the shared ModelTypeStoreService from the
  // profile. Otherwise, the Web Apps system uses its own ModelTypeStoreService
  // instance.
  std::unique_ptr<syncer::ModelTypeStoreService> model_type_store_service_;

  Profile* const profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_
