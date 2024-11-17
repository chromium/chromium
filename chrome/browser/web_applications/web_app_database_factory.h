// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/model/data_type_store.h"

class Profile;

namespace web_app {

class AbstractWebAppDatabaseFactory {
 public:
  virtual ~AbstractWebAppDatabaseFactory() = default;
  virtual syncer::OnceDataTypeStoreFactory GetStoreFactory() = 0;
  virtual bool IsSyncingApps() = 0;
};

class WebAppDatabaseFactory : public AbstractWebAppDatabaseFactory {
 public:
  explicit WebAppDatabaseFactory(Profile* profile);
  WebAppDatabaseFactory(const WebAppDatabaseFactory&) = delete;
  WebAppDatabaseFactory& operator=(const WebAppDatabaseFactory&) = delete;
  ~WebAppDatabaseFactory() override;

  // AbstractWebAppDatabaseFactory implementation.
  syncer::OnceDataTypeStoreFactory GetStoreFactory() override;
  bool IsSyncingApps() override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_FACTORY_H_
