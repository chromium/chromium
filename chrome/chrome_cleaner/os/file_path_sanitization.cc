// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

#include <shlobj.h>

#include <utility>

#include "base/base_paths_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace chrome_cleaner {

namespace sanitization_internal {

using chrome_cleaner::CsidlToPathServiceKey;

// This was added because some tests override the registry causing
// SHGetFolderPath (which was used to test if InitilizeDiskUtil() was called) to
// fail. Eventually it should be replaced by a class which properly handles the
// initialization checking.
//
// The default value of this flag should be false. Once
// InitializeFilePathSanitization()() is
// called, it will be set to true. DCHECK should be used to verify it is true
// whenever the code depends on OffsetCSIDLToPath() being registered with
// PathService().
bool initialize_file_path_sanitization_called = false;

// This data structure represents replacement rules, inorder, used by
// SanitizePath() and tests. It represents the relationship of PathService keys
// to labels. The PathService keys are linked to CSIDL values used in PUP
// footprints.
//
// An offset of PATH_CSIDL_START is added to the CSIDL values to map to a range
// which doesn't collide with other PathService providers.
const int PATH_CSIDL_START = 900;
const int PATH_CSIDL_END = PATH_CSIDL_START + 256;
const struct rewrite_rule_kvpair rewrite_rules[] = {
    {CsidlToPathServiceKey(CSIDL_PROGRAM_FILES_COMMON),
     L"CSIDL_PROGRAM_FILES_COMMON"},

    //                          32-bit   32-bit on 64-bit   64-bit on 64-bit
    // CSIDL_PROGRAM_FILES        1             2                  1
    // CSIDL_PROGRAM_FILESX86     1*            2                  2
    // DIR_PROGRAM_FILES6432      1             1                  1
    // 1 - C:\Program Files    2 - C:\Program Files (x86)
    // *CSIDL_PROGRAM_FILESX86 is not valid for Windows XP.
    {CsidlToPathServiceKey(CSIDL_PROGRAM_FILES), L"CSIDL_PROGRAM_FILES"},
    {CsidlToPathServiceKey(CSIDL_PROGRAM_FILESX86), L"CSIDL_PROGRAM_FILES"},
    {base::DIR_PROGRAM_FILES6432, L"CSIDL_PROGRAM_FILES"},

    // Child of CSIDL_COMMON_PROGRAMS.
    {CsidlToPathServiceKey(CSIDL_COMMON_STARTUP), L"CSIDL_COMMON_STARTUP"},
    // Child of CSIDL_COMMON_STARTMENU.
    {CsidlToPathServiceKey(CSIDL_COMMON_PROGRAMS), L"CSIDL_COMMON_PROGRAMS"},
    // Child of CSIDL_COMMON_APPDATA.
    {CsidlToPathServiceKey(CSIDL_COMMON_STARTMENU), L"CSIDL_COMMON_STARTMENU"},
    {CsidlToPathServiceKey(CSIDL_COMMON_APPDATA), L"CSIDL_COMMON_APPDATA"},

    {CsidlToPathServiceKey(CSIDL_INTERNET_CACHE), L"CSIDL_INTERNET_CACHE"},

    // Child of CSIDL_PROGRAMS.
    {CsidlToPathServiceKey(CSIDL_STARTUP), L"CSIDL_STARTUP"},
    // Child of CSIDL_STARTMENU.
    {CsidlToPathServiceKey(CSIDL_PROGRAMS), L"CSIDL_PROGRAMS"},
    // Child of CSIDL_LOCAL_APP_DATA
    {base::DIR_TEMP, L"%TEMP%"},
    // Child of CSIDL_APPDATA.
    {CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA), L"CSIDL_LOCAL_APPDATA"},
    // Child of CSIDL_APPDATA.
    {CsidlToPathServiceKey(CSIDL_STARTMENU), L"CSIDL_STARTMENU"},
    {CsidlToPathServiceKey(CSIDL_APPDATA), L"CSIDL_APPDATA"},

    // Child of CSIDL_WINDOWS.
    {CsidlToPathServiceKey(CSIDL_SYSTEM), L"CSIDL_SYSTEM"},
    {CsidlToPathServiceKey(CSIDL_SYSTEMX86), L"CSIDL_SYSTEM"},
    {CsidlToPathServiceKey(CSIDL_WINDOWS), L"CSIDL_WINDOWS"},

    // Child of CSIDL_PROFILE.
    {CsidlToPathServiceKey(CSIDL_DESKTOP), L"CSIDL_DESKTOP"},
    // Child of CSIDL_PROFILE.
    {CsidlToPathServiceKey(CSIDL_COMMON_FAVORITES), L"CSIDL_COMMON_FAVORITES"},
    {CsidlToPathServiceKey(CSIDL_PROFILE), L"CSIDL_PROFILE"},

    {CsidlToPathServiceKey(CSIDL_COMMON_DESKTOPDIRECTORY),
     L"CSIDL_COMMON_DESKTOPDIRECTORY"},
    {CsidlToPathServiceKey(CSIDL_COMMON_DOCUMENTS), L"CSIDL_COMMON_DOCUMENTS"},
    {0, nullptr}  // This marks the end of the array.
};

}  // namespace sanitization_internal

namespace {

// To make git cl lint happy.
const size_t kMaxPath = _MAX_PATH;

// Retrieves a special_path from PathService and returns true if path is a child
// path.
bool PathIsChildOfSpecialPath(int special_path_id,
                              const base::FilePath& normalized_path,
                              base::FilePath* special_path) {
  if (!base::PathService::Get(special_path_id, special_path)) {
    return false;
  }
  if (special_path_id < sanitization_internal::PATH_CSIDL_START ||
      special_path_id >= sanitization_internal::PATH_CSIDL_END) {
    base::FilePath lowercase_rule_path =
        base::FilePath(base::ToLowerASCII(special_path->value()));
    // If normalized_path doesn't exist, the conversion to long_path will
    // fail and there may be short path components converted to lowercase in
    // normalized_path. An example where this occurs is base::dir_temp.
    if (lowercase_rule_path.IsParent(normalized_path)) {
      *special_path = lowercase_rule_path;
      return true;
    } else {
      *special_path = NormalizePath(*special_path);
    }
  }
  return special_path->IsParent(normalized_path);
}

base::FilePath SanitizePathImpl(const base::FilePath& path) {
  // This check makes sure InitializeFilePathSanitization()() has already been
  // called.
  base::FilePath rule_path;
  DCHECK(sanitization_internal::initialize_file_path_sanitization_called)
      << "InitializeFilePathSanitization()() must be called before "
         "SanitizePathImpl()";

  base::FilePath normalized_path = NormalizePath(path);
  if (normalized_path.empty())
    return normalized_path;

  for (const auto* rule = sanitization_internal::rewrite_rules;
       rule->path != nullptr; ++rule) {
    if (PathIsChildOfSpecialPath(rule->id, normalized_path, &rule_path)) {
      base::FilePath resulting_path(rule->path);
      if (rule_path.AppendRelativePath(normalized_path, &resulting_path)) {
        return resulting_path;
      } else {
        NOTREACHED() << "AppendRelativePath() failed inside SanitizePathImpl()";
      }
    }
  }

  // Nothing to sanitize, return the original path.
  return normalized_path;
}

// This function is used with PathService::RegisterProvider() to map CSIDL
// values offset by PATH_CSIDL_START to their paths using the caching provided
// by PathService without state problems during unit tests.
bool OffsetCSIDLToPath(int csidl_with_offset, base::FilePath* path) {
  if (csidl_with_offset < sanitization_internal::PATH_CSIDL_START ||
      csidl_with_offset >= sanitization_internal::PATH_CSIDL_END) {
    return false;
  }
  wchar_t special_folder_path[kMaxPath];
  HRESULT hr = ::SHGetFolderPath(
      nullptr, csidl_with_offset - sanitization_internal::PATH_CSIDL_START,
      nullptr, SHGFP_TYPE_CURRENT, special_folder_path);
  if (hr == S_OK) {
    *path = NormalizePath(base::FilePath(special_folder_path));
    return true;
  }
  return false;
}

}  // namespace

void InitializeFilePathSanitization() {
  // Only do this once.
  static bool init_once = []() -> bool {
    // Setup PathService to use OffsetCSIDLToPath so that SanitizePath can
    // benefit from the caching provided in PathService.
    base::PathService::RegisterProvider(&OffsetCSIDLToPath,
                                        sanitization_internal::PATH_CSIDL_START,
                                        sanitization_internal::PATH_CSIDL_END);

    // Cache Paths to prevent concurrent calls to SHGetFolderPath.
    for (int path_id = sanitization_internal::PATH_CSIDL_START;
         path_id < sanitization_internal::PATH_CSIDL_END; ++path_id) {
      base::FilePath tempPath;
      base::PathService::Get(path_id, &tempPath);
    }

    sanitization_internal::initialize_file_path_sanitization_called = true;
    return true;
  }();
  ANALYZER_ALLOW_UNUSED(init_once);
}

std::vector<base::FilePath> GetRewrittenPaths() {
  std::vector<base::FilePath> paths;
  for (const auto* rule = sanitization_internal::rewrite_rules;
       rule->path != nullptr; ++rule) {
    base::FilePath rule_path;
    if (base::PathService::Get(rule->id, &rule_path))
      paths.push_back(NormalizePath(rule_path));
  }
  return paths;
}

std::map<int, base::string16> PathKeyToSanitizeString() {
  std::map<int, base::string16> path_key_to_sanitize_string;
  for (const auto* rule = sanitization_internal::rewrite_rules;
       rule->path != nullptr; ++rule) {
    path_key_to_sanitize_string.insert(std::make_pair(rule->id, rule->path));
  }
  return path_key_to_sanitize_string;
}

int CsidlToPathServiceKey(int CSIDL) {
  return sanitization_internal::PATH_CSIDL_START + CSIDL;
}

base::FilePath NormalizePath(const base::FilePath& path) {
  base::string16 long_path;
  ConvertToLongPath(path.value(), &long_path);
  return base::FilePath(base::ToLowerASCII(long_path));
}

void ConvertToLongPath(const base::string16& path, base::string16* long_path) {
  DCHECK(long_path);
  DWORD long_path_len = ::GetLongPathName(path.c_str(), nullptr, 0);
  if (long_path_len > 0UL) {
    long_path_len = ::GetLongPathName(
        path.c_str(), base::WriteInto(long_path, long_path_len), long_path_len);
    DCHECK_GT(long_path_len, 0UL);
  } else {
    *long_path = path;
  }
}

base::string16 SanitizePath(const base::FilePath& path) {
  return SanitizePathImpl(path).value();
}

base::string16 SanitizeCommandLine(const base::CommandLine& command_line) {
  base::FilePath sanitized_program =
      SanitizePathImpl(command_line.GetProgram());
  base::CommandLine sanitized_command_line(sanitized_program);
  for (const auto& s : command_line.GetSwitches()) {
    sanitized_command_line.AppendSwitchNative(
        s.first, SanitizePath(base::FilePath(s.second)));
  }
  for (const auto& arg : command_line.GetArgs()) {
    sanitized_command_line.AppendArgNative(SanitizePath(base::FilePath(arg)));
  }
  return sanitized_command_line.GetCommandLineString();
}

base::FilePath ExpandSpecialFolderPath(int csidl,
                                       const base::FilePath& input_path) {
  // This check makes sure ExpandSpecialFolderPath() has already been called.
  base::FilePath special_folder_path;
  DCHECK(sanitization_internal::initialize_file_path_sanitization_called)
      << "InitializeFilePathSanitization()() must be called before "
         "ExpandSpecialFolderPath()";
  // No need to expand an absolute path, |csidl| is simply ignored in that case.
  if (input_path.IsAbsolute())
    return input_path;

  if (base::PathService::Get(CsidlToPathServiceKey(csidl),
                             &special_folder_path)) {
    return base::FilePath(special_folder_path).Append(input_path);
  }

  return base::FilePath();
}

bool ValidateSandboxFilePath(const base::FilePath& file_path) {
  const base::FilePath::StringType& path_string = file_path.value();
  if (path_string.empty()) {
    LOG(ERROR) << "File path cannot be empty";
    return false;
  }
  // Disallow UNC paths (\\ServerName\...) and paths with the universal \\?\
  // prefix described at
  // https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file#namespaces
  if (path_string.length() >= 2 &&
      base::FilePath::IsSeparator(path_string[0]) &&
      base::FilePath::IsSeparator(path_string[1])) {
    // Don't print the path because SanitizePath is not safe to run on UNC
    // paths.
    LOG(ERROR) << "File path must not start with \\\\";
    return false;
  }
  // Disallow paths with the native prefix (\??\) described at
  // https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html.
  if (path_string.length() >= 4 &&
      base::FilePath::IsSeparator(path_string[0]) && path_string[1] == '?' &&
      path_string[2] == '?' && base::FilePath::IsSeparator(path_string[3])) {
    // Don't print the path because SanitizePath is not safe to run on native
    // paths.
    LOG(ERROR) << "File path must not start with \\??\\";
    return false;
  }
  if (!file_path.IsAbsolute()) {
    LOG(ERROR) << "File path must be absolute, received "
               << SanitizePath(file_path);
    return false;
  }
  return true;
}

bool IsLocalFileAttributes(DWORD file_attributes) {
  return !(file_attributes == INVALID_FILE_ATTRIBUTES ||
           file_attributes & FILE_ATTRIBUTE_OFFLINE ||
           file_attributes & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS ||
           file_attributes & FILE_ATTRIBUTE_RECALL_ON_OPEN);
}

bool IsFilePresentLocally(const base::FilePath& file_name) {
  DWORD file_attributes = ::GetFileAttributes(file_name.value().c_str());
  if (file_attributes == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "IsFilePresentLocally failed to get attributes: "
                << SanitizePath(file_name);
    return false;
  }
  return IsLocalFileAttributes(file_attributes);
}

}  // namespace chrome_cleaner
