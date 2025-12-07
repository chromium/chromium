// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/preloaded_isolated_origins.h"

#include "base/strings/string_tokenizer.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/site_isolation_policy.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace site_isolation {

std::vector<url::Origin> GetBrowserSpecificBuiltInIsolatedOrigins() {
  std::vector<url::Origin> list;

  // Only apply preloaded isolated origins when allowed by site isolation
  // policy (e.g., when memory requirements are satisfied, and when not using
  // full site isolation).
  if (content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled()) {
    std::string newline_separted_origins =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_ISOLATED_ORIGINS);
    base::StringTokenizer t(newline_separted_origins, "\n");
    while (std::optional<std::string_view> token = t.GetNextTokenView()) {
      list.push_back(url::Origin::Create(GURL(*token)));
    }
  }

  return list;
}

}  // namespace site_isolation
