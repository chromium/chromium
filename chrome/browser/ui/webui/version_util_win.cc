// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_util_win.h"

#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "chrome/install_static/install_details.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace version_utils {
namespace win {

std::string GetFullWindowsVersion() {
  std::string version;
  base::win::OSInfo* gi = base::win::OSInfo::GetInstance();
  const int major = gi->version_number().major;
  const int minor = gi->version_number().minor;
  const int build = gi->version_number().build;
  const int patch = gi->version_number().patch;
  // Server or Desktop
  const bool server =
      gi->version_type() == base::win::VersionType::SUITE_SERVER;
  // Service Pack
  const std::string sp = gi->service_pack_str();

  if (major == 10) {
    version += (server) ? "Server OS" : "10 OS";
  } else if (major == 6) {
    switch (minor) {
      case 0:
        // Windows Vista or Server 2008
        version += (server) ? "Server 2008 " : "Vista ";
        version += sp;
        break;
      case 1:
        // Windows 7 or Server 2008 R2
        version += (server) ? "Server 2008 R2 " : "7 ";
        version += sp;
        break;
      case 2:
        // Windows 8 or Server 2012
        version += (server) ? "Server 2012" : "8";
        break;
      case 3:
        // Windows 8.1 or Server 2012 R2
        version += (server) ? "Server 2012 R2" : "8.1";
        break;
      default:
        // unknown version
        return base::StringPrintf("unknown version 6.%d", minor);
    }
  } else if ((major == 5) && (minor > 0)) {
    // Windows XP or Server 2003
    version += (server) ? "Server 2003 " : "XP ";
    version += sp;
  } else {
    // unknown version
    return base::StringPrintf("unknown version %d.%d", major, minor);
  }

  const std::string release_id = gi->release_id();

  if (!release_id.empty())
    version += " Version " + release_id;

  if (patch > 0)
    version += base::StringPrintf(" (Build %d.%d)", build, patch);
  else
    version += base::StringPrintf(" (Build %d)", build);
  return version;
}

base::string16 GetCohortVersionInfo() {
  base::string16 update_cohort_name =
      install_static::InstallDetails::Get().update_cohort_name();
  if (!update_cohort_name.empty()) {
    return l10n_util::GetStringFUTF16(IDS_VERSION_UI_COHORT_NAME,
                                      update_cohort_name);
  }

  return base::string16();
}

}  // namespace win
}  // namespace version_utils
