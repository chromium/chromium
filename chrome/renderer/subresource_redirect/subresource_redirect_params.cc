// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

url::Origin GetSubresourceRedirectOrigin() {
  auto lite_page_subresource_origin = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "lite_page_subresource_origin");
  if (lite_page_subresource_origin.empty())
    return url::Origin::Create(GURL("https://litepages.googlezip.net/"));
  return url::Origin::Create(GURL(lite_page_subresource_origin));
}

}  // namespace subresource_redirect
