// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"

namespace webui_browser {

bool IsWebUIBrowserEnabled() {
  return base::FeatureList::IsEnabled(features::kWebium);
}

}  // namespace webui_browser
