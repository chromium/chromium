// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "chrome/browser/browser_process.h"

namespace {

// Hardcoded value for the secure version of Acrobat Reader.
const char kSecureVersion[] = "11.0.8.4";

const base::char16 kRegistryAcrobat[] = L"Acrobat.exe";
const base::char16 kRegistryAcrobatReader[] = L"AcroRd32.exe";
const base::char16 kRegistryApps[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const base::char16 kRegistryPath[] = L"Path";

// Gets the installed path for a registered app.
base::FilePath GetInstalledPath(const base::char16* app) {
  base::string16 reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  base::FilePath filepath;
  base::win::RegKey hkcu_key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  base::string16 path;
  // AppPaths can also be registered in HKCU: http://goo.gl/UgFOf.
  if (hkcu_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
    filepath = base::FilePath(path);
  } else {
    base::win::RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (hklm_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
      filepath = base::FilePath(path);
    }
  }
  return filepath.Append(app);
}

bool IsAdobeReaderDefaultPDFViewerInternal(base::FilePath* path) {
  base::char16 app_cmd_buf[MAX_PATH];
  DWORD app_cmd_buf_len = MAX_PATH;
  HRESULT hr = AssocQueryString(ASSOCF_NONE, ASSOCSTR_COMMAND, L".pdf", L"open",
                                app_cmd_buf, &app_cmd_buf_len);
  if (FAILED(hr))
    return false;

  // Looks for the install paths for Acrobat / Reader.
  base::FilePath install_path = GetInstalledPath(kRegistryAcrobatReader);
  if (install_path.empty())
    install_path = GetInstalledPath(kRegistryAcrobat);
  if (install_path.empty())
    return false;

  base::string16 app_cmd(app_cmd_buf);
  bool found = app_cmd.find(install_path.value()) != base::string16::npos;
  if (found && path)
    *path = install_path;
  return found;
}

}  // namespace

bool IsAdobeReaderDefaultPDFViewer() {
  return IsAdobeReaderDefaultPDFViewerInternal(NULL);
}

bool IsAdobeReaderUpToDate() {
  base::FilePath install_path;
  bool is_default = IsAdobeReaderDefaultPDFViewerInternal(&install_path);
  if (!is_default)
    return false;

  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(install_path));
  if (!file_version_info)
    return false;

  std::string reader_version =
      base::UTF16ToUTF8(file_version_info->product_version());
  // Convert 1.2.03.45 to 1.2.3.45 so base::Version considers it as valid.
  for (int i = 1; i <= 9; ++i) {
    std::string from = base::StringPrintf(".0%d", i);
    std::string to = base::StringPrintf(".%d", i);
    base::ReplaceSubstringsAfterOffset(&reader_version, 0, from, to);
  }
  base::Version file_version(reader_version);
  return file_version.IsValid() &&
    file_version >= base::Version(kSecureVersion);
}
