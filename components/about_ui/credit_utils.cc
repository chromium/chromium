// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/about_ui/credit_utils.h"

#include <stdint.h>

#include "build/chromeos_buildflags.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace about_ui {

std::string GetCredits(bool include_scripts) {
  std::string response =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_ABOUT_UI_CREDITS_HTML);
  if (include_scripts) {
    response +=
        "<script src=\"chrome://credits/credits.js\"></script>\n";
  }
  response += "</body>\n</html>";
  return response;
}

}  // namespace about_ui
