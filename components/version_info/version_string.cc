// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/version_string.h"

#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"

#if defined(USE_UNOFFICIAL_VERSION_NUMBER)
#include "ui/base/l10n/l10n_util.h"  // nogncheck
#endif  // USE_UNOFFICIAL_VERSION_NUMBER

namespace version_info {

std::string GetVersionStringWithModifier(const std::string& modifier) {
  std::string current_version;
  current_version += GetVersionNumber();
#if defined(USE_UNOFFICIAL_VERSION_NUMBER)
  current_version += " (";
  current_version += l10n_util::GetStringUTF8(IDS_VERSION_UI_UNOFFICIAL);
  current_version += " ";
  current_version += GetLastChange();
  current_version += " ";
  current_version += GetOSType();
  current_version += ")";
#endif  // USE_UNOFFICIAL_VERSION_NUMBER
  if (!modifier.empty())
    current_version += " " + modifier;
  return current_version;
}

}  // namespace version_info
