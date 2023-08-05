// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"

#include <string>

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.h"

namespace chromeos::settings {

namespace {

std::string RemoveQuery(std::string path) {
  std::string::size_type input_index = path.find('?');
  if (input_index != std::string::npos)
    path.resize(input_index);
  return path;
}

}  // namespace

bool IsOSSettingsSubPage(const std::string& sub_page) {
  // Sub-pages may have query parameters, e.g. networkDetail?guid=123456.
  std::string sub_page_without_query = RemoveQuery(sub_page);

  for (const char* p : kPaths) {
    std::string path_without_query = RemoveQuery(p);
    if (sub_page_without_query == path_without_query)
      return true;
  }

  return false;
}

}  // namespace chromeos::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
