// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"

#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

bool HasPinnedHomeTab(const TabStripModel* tab_strip_model) {
  if (!tab_strip_model->ContainsIndex(0))
    return false;
  return tab_strip_model->delegate()->IsForWebApp() &&
         tab_strip_model->IsTabPinned(0);
}

bool IsPinnedHomeTab(const TabStripModel* tab_strip_model, int index) {
  return HasPinnedHomeTab(tab_strip_model) && index == 0;
}

}  // namespace web_app
