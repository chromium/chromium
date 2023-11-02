// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/model/model_type_store_service.h"

namespace web_app {

WebAppDatabaseFactory::WebAppDatabaseFactory(Profile* profile)
    : profile_(profile) {}

WebAppDatabaseFactory::~WebAppDatabaseFactory() = default;

syncer::OnceModelTypeStoreFactory WebAppDatabaseFactory::GetStoreFactory() {
  return ModelTypeStoreServiceFactory::GetForProfile(profile_)
      ->GetStoreFactory();
}

}  // namespace web_app
