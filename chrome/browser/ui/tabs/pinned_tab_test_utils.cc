// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"

namespace {

std::string TabToString(const StartupTab& tab) {
  std::string type_description;
  switch (tab.type) {
    case StartupTab::Type::kNormal:
      break;
    case StartupTab::Type::kPinned:
      type_description = "pinned";
      break;
    case StartupTab::Type::kFromLastAndUrlsStartupPref:
      type_description = "from LAST_AND_URLS startup pref";
      break;
  }
  return tab.url.spec() + ":" + type_description;
}

}  // namespace

// static
std::string PinnedTabTestUtils::TabsToString(
    const std::vector<StartupTab>& values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0)
      result += " ";
    result += TabToString(values[i]);
  }
  return result;
}
