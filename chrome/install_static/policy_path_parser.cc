// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/install_static/policy_path_parser.h"

#include <assert.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdlib.h>
#include <wtsapi32.h>

#include <memory>

namespace {

constexpr WCHAR kMachineNamePolicyVarName[] = L"${machine_name}";
constexpr WCHAR kUserNamePolicyVarName[] = L"${user_name}";
constexpr WCHAR kWinDocumentsFolderVarName[] = L"${documents}";
constexpr WCHAR kWinLocalAppDataFolderVarName[] = L"${local_app_data}";
constexpr WCHAR kWinRoamingAppDataFolderVarName[] = L"${roaming_app_data}";
constexpr WCHAR kWinProfileFolderVarName[] = L"${profile}";
constexpr WCHAR kWinProgramDataFolderVarName[] = L"${global_app_data}";
constexpr WCHAR kWinProgramFilesFolderVarName[] = L"${program_files}";
constexpr WCHAR kWinWindowsFolderVarName[] = L"${windows}";
constexpr WCHAR kWinClientName[] = L"${client_name}";
constexpr WCHAR kWinSessionName[] = L"${session_name}";

struct WinFolderNamesToCSIDLMapping {
  const WCHAR* name;
  int id;
};

// Mapping from variable names to Windows CSIDL ids.
constexpr WinFolderNamesToCSIDLMapping kWinFolderMapping[] = {
    {kWinWindowsFolderVarName, CSIDL_WINDOWS},
    {kWinProgramFilesFolderVarName, CSIDL_PROGRAM_FILES},
    {kWinProgramDataFolderVarName, CSIDL_COMMON_APPDATA},
    {kWinProfileFolderVarName, CSIDL_PROFILE},
    {kWinLocalAppDataFolderVarName, CSIDL_LOCAL_APPDATA},
    {kWinRoamingAppDataFolderVarName, CSIDL_APPDATA},
    {kWinDocumentsFolderVarName, CSIDL_PERSONAL}};

template <class FunctionType>
struct ScopedFunctionHelper {
  ScopedFunctionHelper(const wchar_t* library_name, const char* function_name) {
    library_ = LoadLibrary(library_name);
    assert(library_);
    if (library_) {
      // Strip off any leading :: that may have come from stringifying the
      // function's name.
      if (function_name[0] == ':' && function_name[1] == ':' &&
          function_name[2] && function_name[2] != ':') {
        function_name += 2;
      }
      function_ = reinterpret_cast<FunctionType*>(
          GetProcAddress(library_, function_name));
      assert(function_);
    }
  }

  ~ScopedFunctionHelper() {
    if (library_)
      FreeLibrary(library_);
  }

  template <class... Args>
  auto operator()(Args... a) {
    return function_(a...);
  }

 private:
  HMODULE library_;
  FunctionType* function_;
};

#define SCOPED_LOAD_FUNCTION(library, function) \
  ScopedFunctionHelper<decltype(function)>(library, #function)

}  // namespace

namespace install_static {

// Replaces all variable occurrences in the policy string with the respective
// system settings values.
// Note that this uses GetProcAddress to load DLLs that cannot be loaded before
// the blacklist in the DllMain of chrome_elf has been applied. This function
// should only be used after DllMain() has run.
std::wstring ExpandPathVariables(const std::wstring& untranslated_string) {
  std::wstring result(untranslated_string);
  if (result.length() == 0)
    return result;
  // Sanitize quotes in case of any around the whole string.
  if (result.length() > 1 &&
      ((result.front() == L'"' && result.back() == L'"') ||
       (result.front() == L'\'' && result.back() == L'\''))) {
    // Strip first and last char which should be matching quotes now.
    result.pop_back();
    result.erase(0, 1);
  }
  auto sh_get_special_folder_path =
      SCOPED_LOAD_FUNCTION(L"shell32.dll", ::SHGetSpecialFolderPathW);
  // First translate all path variables we recognize.
  for (size_t i = 0; i < _countof(kWinFolderMapping); ++i) {
    size_t position = result.find(kWinFolderMapping[i].name);
    if (position != std::wstring::npos) {
      size_t variable_length = wcslen(kWinFolderMapping[i].name);
      WCHAR path[MAX_PATH];
      if (!sh_get_special_folder_path(nullptr, path, kWinFolderMapping[i].id,
                                      false)) {
        path[0] = 0;
      }
      std::wstring path_string(path);
      // Remove a trailing slash if there is any but also only if the rest of
      // the string contains one right after to avoid ending in a drive only
      // value situation. This usually won't happen but if the value of this
      // special folder is the root of a drive it will be presented as D:\.
      if (!path_string.empty() && path_string.back() == L'\\' &&
          result.length() > position + variable_length &&
          result[position + variable_length] == L'\\') {
        path_string.pop_back();
      }
      result.replace(position, variable_length, path_string);
    }
  }
  // Next translate other windows specific variables.
  auto get_user_name = SCOPED_LOAD_FUNCTION(L"advapi32.dll", ::GetUserNameW);
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::wstring::npos) {
    DWORD return_length = 0;
    get_user_name(nullptr, &return_length);
    if (return_length != 0) {
      std::unique_ptr<WCHAR[]> username(new WCHAR[return_length]);
      get_user_name(username.get(), &return_length);
      std::wstring username_string(username.get());
      result.replace(position, wcslen(kUserNamePolicyVarName), username_string);
    }
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::wstring::npos) {
    DWORD return_length = 0;
    ::GetComputerNameEx(ComputerNamePhysicalDnsHostname, nullptr,
                        &return_length);
    if (return_length != 0) {
      std::unique_ptr<WCHAR[]> machinename(new WCHAR[return_length]);
      ::GetComputerNameEx(ComputerNamePhysicalDnsHostname, machinename.get(),
                          &return_length);
      std::wstring machinename_string(machinename.get());
      result.replace(position, wcslen(kMachineNamePolicyVarName),
                     machinename_string);
    }
  }
  auto wts_query_session_information =
      SCOPED_LOAD_FUNCTION(L"wtsapi32.dll", ::WTSQuerySessionInformationW);
  auto wts_free_memory = SCOPED_LOAD_FUNCTION(L"wtsapi32.dll", ::WTSFreeMemory);
  position = result.find(kWinClientName);
  if (position != std::wstring::npos) {
    LPWSTR buffer = nullptr;
    DWORD buffer_length = 0;
    if (wts_query_session_information(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                      WTSClientName, &buffer, &buffer_length)) {
      std::wstring clientname_string(buffer);
      result.replace(position, wcslen(kWinClientName), clientname_string);
      wts_free_memory(buffer);
    }
  }
  position = result.find(kWinSessionName);
  if (position != std::wstring::npos) {
    LPWSTR buffer = nullptr;
    DWORD buffer_length = 0;
    if (wts_query_session_information(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                      WTSWinStationName, &buffer,
                                      &buffer_length)) {
      std::wstring sessionname_string(buffer);
      result.replace(position, wcslen(kWinSessionName), sessionname_string);
      wts_free_memory(buffer);
    }
  }
  // TODO(pastarmovj): Consider reorganizing this code once there are even more
  // variables to be supported. The search for the var and its replacement can
  // be extracted as common functionality.

  return result;
}

}  // namespace install_static
