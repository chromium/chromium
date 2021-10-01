// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_APPCACHE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_APPCACHE_HELPER_H_

#include <list>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/browsing_data/content/appcache_helper.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

class MockAppCacheHelper : public AppCacheHelper {
 public:
  explicit MockAppCacheHelper(content::BrowserContext* browser_context);

  MockAppCacheHelper(const MockAppCacheHelper&) = delete;
  MockAppCacheHelper& operator=(const MockAppCacheHelper&) = delete;

  void StartFetching(FetchCallback completion_callback) override;
  void DeleteAppCaches(const url::Origin& origin) override;

  // Adds AppCache samples.
  void AddAppCacheSamples();

  // Notifies the callback.
  void Notify();

 private:
  ~MockAppCacheHelper() override;

  FetchCallback completion_callback_;

  std::list<content::StorageUsageInfo> response_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_APPCACHE_HELPER_H_
