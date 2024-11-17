// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/model/data_type_store_service.h"

namespace web_app {

WebAppDatabaseFactory::WebAppDatabaseFactory(Profile* profile)
    : profile_(profile) {}

WebAppDatabaseFactory::~WebAppDatabaseFactory() = default;

syncer::OnceDataTypeStoreFactory WebAppDatabaseFactory::GetStoreFactory() {
  return DataTypeStoreServiceFactory::GetForProfile(profile_)
      ->GetStoreFactory();
}

bool WebAppDatabaseFactory::IsSyncingApps() {
  return IsSyncEnabledForApps(profile_);
}

}  // namespace web_app
