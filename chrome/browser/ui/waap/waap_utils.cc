// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

bool IsForInitialWebUI(const GURL& url) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI) &&
      base::FeatureList::IsEnabled(features::kWebUIReloadButton)) {
    return url.SchemeIs(content::kChromeUIScheme) &&
           url.host() == chrome::kChromeUIReloadButtonHost;
  }
  return false;
}
