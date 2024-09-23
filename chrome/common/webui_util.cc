// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_util.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace chrome {

bool ShouldUseCodeCacheForWebUIUrl(const GURL& request_url) {
  DCHECK(content::HasWebUIScheme(request_url));
  if (base::FeatureList::IsEnabled(features::kRestrictedWebUICodeCache)) {
    static const base::NoDestructor<base::flat_set<std::string>>
        unrestricted_resources(base::SplitString(
            features::kRestrictedWebUICodeCacheResources.Get(), ",",
            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
    return unrestricted_resources->contains(request_url.path());
  }
  return true;
}

}  // namespace chrome
