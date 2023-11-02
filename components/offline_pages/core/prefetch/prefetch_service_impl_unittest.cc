// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

TEST(PrefetchServiceTest, ServiceDoesNotCrash) {
  PrefetchServiceImpl service(nullptr);

  service.AddCandidatePrefetchURLs(std::vector<PrefetchService::PrefetchURL>());
  service.RemoveAllUnprocessedPrefetchURLs(kSuggestedArticlesNamespace);
  service.RemovePrefetchURLsByClientId({kSuggestedArticlesNamespace, "123"});
}

}  // namespace offline_pages
