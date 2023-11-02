// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_remover_allowlist.h"

#include <shlobj.h>

#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

namespace {

const int kCsidlAllowlist[] = {
    CSIDL_ADMINTOOLS,
    CSIDL_APPDATA,
    CSIDL_COMMON_ADMINTOOLS,
    CSIDL_COMMON_ALTSTARTUP,
    CSIDL_COMMON_APPDATA,
    CSIDL_COMMON_DESKTOPDIRECTORY,
    CSIDL_COMMON_DOCUMENTS,
    CSIDL_COMMON_FAVORITES,
    CSIDL_COMMON_MUSIC,
    CSIDL_COMMON_OEM_LINKS,
    CSIDL_COMMON_PICTURES,
    CSIDL_COMMON_PROGRAMS,
    CSIDL_COMMON_STARTMENU,
    CSIDL_COMMON_STARTUP,
    CSIDL_COMMON_TEMPLATES,
    CSIDL_COMMON_VIDEO,
    CSIDL_COMPUTERSNEARME,
    CSIDL_CONNECTIONS,
    CSIDL_CONTROLS,
    CSIDL_COOKIES,
    CSIDL_DESKTOP,
    CSIDL_DESKTOPDIRECTORY,
    CSIDL_DRIVES,
    CSIDL_FAVORITES,
    CSIDL_FONTS,
    CSIDL_HISTORY,
    CSIDL_INTERNET,
    CSIDL_INTERNET_CACHE,
    CSIDL_LOCAL_APPDATA,
    CSIDL_MYDOCUMENTS,
    CSIDL_MYMUSIC,
    CSIDL_MYPICTURES,
    CSIDL_MYVIDEO,
    CSIDL_NETHOOD,
    CSIDL_NETWORK,
    CSIDL_PERSONAL,
    CSIDL_PRINTERS,
    CSIDL_PRINTHOOD,
    CSIDL_PROFILE,
    CSIDL_PROGRAM_FILES,
    CSIDL_PROGRAM_FILES_COMMON,
    CSIDL_PROGRAM_FILES_COMMONX86,
    CSIDL_PROGRAM_FILESX86,
    CSIDL_PROGRAMS,
    CSIDL_RECENT,
    CSIDL_RESOURCES,
    CSIDL_RESOURCES_LOCALIZED,
    CSIDL_SENDTO,
    CSIDL_STARTMENU,
    CSIDL_STARTUP,
    CSIDL_SYSTEM,
    CSIDL_SYSTEMX86,
    CSIDL_TEMPLATES,
    CSIDL_WINDOWS,
};

}  // namespace

// static
FileRemoverAllowlist* FileRemoverAllowlist::GetInstance() {
  return base::Singleton<FileRemoverAllowlist>::get();
}

FileRemoverAllowlist::~FileRemoverAllowlist() = default;

void FileRemoverAllowlist::DisableCache() {
  cache_disabled_ = true;
}

bool FileRemoverAllowlist::IsAllowlisted(const base::FilePath& path) {
  if (cache_disabled_) {
    GenerateFileRemoverAllowlist();
  }

  return allowlisted_paths_.find(NormalizePath(path)) !=
         allowlisted_paths_.end();
}

FileRemoverAllowlist::FileRemoverAllowlist() {
  GenerateFileRemoverAllowlist();
}

void FileRemoverAllowlist::GenerateFileRemoverAllowlist() {
  allowlisted_paths_.clear();

  // Ensure no footprint are the root of a CSIDL.
  base::FilePath empty_path;
  for (int csidl : kCsidlAllowlist) {
    base::FilePath expanded_csidl = ExpandSpecialFolderPath(csidl, empty_path);
    if (!expanded_csidl.empty())
      allowlisted_paths_.insert(NormalizePath(expanded_csidl));
  }

  for (const base::FilePath& path : GetRewrittenPaths())
    allowlisted_paths_.insert(path);
}

}  // namespace chrome_cleaner
