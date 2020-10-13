// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/common/subresource_filter_utils.h"

#include "content/public/common/url_utils.h"

namespace subresource_filter {

bool ShouldInheritActivation(const GURL& url) {
  return !content::IsURLHandledByNetworkStack(url);
}

}  // namespace subresource_filter
