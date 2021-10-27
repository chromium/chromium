// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/port_monitor/port_monitor.h"

#include <windows.h>
#include <lmcons.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>
#include <strsafe.h>
#include <userenv.h>
#include <winspool.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/virtual_driver/win/port_monitor/spooler_win.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace cloud_print {

namespace {

const wchar_t kIePath[] = L"Internet Explorer\\iexplore.exe";

const char kChromeInstallUrl[] =
    "https://google.com/cloudprint/learn/chrome.html";

const wchar_t kCloudPrintRegKey[] = L"Software\\Google\\CloudPrint";

const wchar_t kXpsMimeType[] = L"application/vnd.ms-xpsdocument";

const wchar_t kAppDataDir[] = L"Google\\Cloud Printer";

const wchar_t kDocumentPathPlaceHolder[] = L"%%Document_Path%%";

const wchar_t kDocumentTypePlaceHolder[] = L"%%Document_Type%%";

const wchar_t kJobTitlePlaceHolder[] = L"%%Job_Title%%";

struct MonitorData {
  std::unique_ptr<base::AtExitManager> at_exit_manager;
};

struct PortData {
  PortData() : job_id(0), printer_handle(NULL), file(nullptr) {}
  ~PortData() { Close(); }
  void Close() {
    if (printer_handle) {
      ClosePrinter(printer_handle);
      printer_handle = NULL;
    }
    if (file) {
      base::CloseFile(file);
      file = nullptr;
    }
  }
  DWORD job_id;
  HANDLE printer_handle;
  raw_ptr<FILE> file;
  base::FilePath file_path;
};

typedef struct { ACCESS_MASK granted_access; } XcvUiData;

MONITORUI g_monitor_ui = {sizeof(MONITORUI), MonitorUiAddPortUi,
                          MonitorUiConfigureOrDeletePortUI,
                          MonitorUiConfigureOrDeletePortUI};

MONITOR2 g_monitor_2 = {sizeof(MONITOR2),
                        Monitor2EnumPorts,
                        Monitor2OpenPort,
                        nullptr,  // OpenPortEx is not supported.
                        Monitor2StartDocPort,
                        Monitor2WritePort,
                        Monitor2ReadPort,
                        Monitor2EndDocPort,
                        Monitor2ClosePort,
                        nullptr,  // AddPort is not supported.
                        nullptr,  // AddPortEx is not supported.
                        nullptr,  // ConfigurePort is not supported.
                        nullptr,  // DeletePort is not supported.
                        nullptr,
                        nullptr,  // SetPortTimeOuts is not supported.
                        Monitor2XcvOpenPort,
                        Monitor2XcvDataPort,
                        Monitor2XcvClosePort,
                        Monitor2Shutdown};

base::FilePath GetLocalAppDataLow() {
  wchar_t system_buffer[MAX_PATH];
  if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                             system_buffer)))
    return base::FilePath();
  return base::FilePath(system_buffer).DirName().AppendASCII("LocalLow");
}

base::FilePath GetAppDataDir() {
  base::FilePath file_path = GetLocalAppDataLow();
  if (file_path.empty()) {
    LOG(ERROR) << "Can't get app data dir";
  }
  return file_path.Append(kAppDataDir);
}

// Delete files which where not deleted by chrome.
void DeleteLeakedFiles(const base::FilePath& dir) {
  base::Time delete_before = base::Time::Now() - base::Days(1);
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    if (enumerator.GetInfo().GetLastModifiedTime() < delete_before)
      base::DeleteFile(file_path);
  }
}

// Attempts to retrieve the title of the specified print job.
// On success returns TRUE and the first title_chars characters of the job title
// are copied into title.
// On failure returns FALSE and title is unmodified.
bool GetJobTitle(HANDLE printer_handle, DWORD job_id, std::wstring* title) {
  DCHECK(printer_handle != NULL);
  DCHECK(title != NULL);
  DWORD bytes_needed = 0;
  GetJob(printer_handle, job_id, 1, NULL, 0, &bytes_needed);
  if (bytes_needed == 0) {
    LOG(ERROR) << "Unable to get bytes needed for job info.";
    return false;
  }
  std::unique_ptr<BYTE[]> buffer(new BYTE[bytes_needed]);
  if (!GetJob(printer_handle, job_id, 1, buffer.get(), bytes_needed,
              &bytes_needed)) {
    LOG(ERROR) << "Unable to get job info.";
    return false;
  }
  JOB_INFO_1* job_info = reinterpret_cast<JOB_INFO_1*>(buffer.get());
  *title = job_info->pDocument;
  return true;
}

// Handler for the UI functions exported by the port monitor.
// Verifies that a valid parent Window exists and then just displays an
// error message to let the user know that there is no interactive
// configuration.
void HandlePortUi(HWND hwnd, const std::wstring& caption) {
  if (hwnd != NULL && IsWindow(hwnd)) {
    DisplayWindowsMessage(hwnd, CO_E_NOT_SUPPORTED, cloud_print::kPortName);
  }
}

// Gets the primary token for the user that submitted the print job.
bool GetUserToken(HANDLE* primary_token) {
  HANDLE token = NULL;
  if (!OpenThreadToken(GetCurrentThread(),
                       TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY,
                       FALSE, &token)) {
    LOG(ERROR) << "Unable to get thread token.";
    return false;
  }
  base::win::ScopedHandle token_scoped(token);
  if (!DuplicateTokenEx(
          token, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY, NULL,
          SecurityImpersonation, TokenPrimary, primary_token)) {
    LOG(ERROR) << "Unable to get primary thread token.";
    return false;
  }
  return true;
}

bool LaunchCommandAsUser(const base::CommandLine& command) {
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    LOG(ERROR) << "Unable to get user token.";
    return false;
  }
  base::win::ScopedHandle primary_token_scoped(token);
  base::LaunchOptions options;
  options.as_user = primary_token_scoped.Get();
  base::LaunchProcess(command, options);
  return true;
}

// Escape the command line argument as necessary per Microsoft rules.
// See QuoteForCommandLineToArgvW in base/command_line.cc
std::wstring EscapeCommandLineArg(const std::wstring& arg) {
  std::wstring quotable_chars(L" \\\"");
  if (arg.find_first_of(quotable_chars) == std::wstring::npos) {
    // No quoting necessary.
    return arg;
  }

  std::wstring out;
  out.push_back(L'"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end) {
      }
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}

// Launch the print command as specified in the cloud print registry.
bool LaunchPrintCommandFromTemplate(const std::wstring& command_template,
                                    const base::FilePath& xps_path,
                                    const std::wstring& job_title) {
  std::wstring command_string(command_template);
  // Substitude the place holder with the document path wrapped in quotes.
  base::ReplaceFirstSubstringAfterOffset(
      &command_string, 0, kDocumentPathPlaceHolder,
      EscapeCommandLineArg(xps_path.value()));
  // Substitude the place holder with the document type wrapped in quotes.
  base::ReplaceFirstSubstringAfterOffset(
      &command_string, 0, kDocumentTypePlaceHolder, kXpsMimeType);
  // Substitude the place holder with the job title wrapped in quotes.
  base::ReplaceFirstSubstringAfterOffset(&command_string, 0,
                                         kJobTitlePlaceHolder,
                                         EscapeCommandLineArg(job_title));

  base::CommandLine command = base::CommandLine::FromString(command_string);

  return LaunchCommandAsUser(command);
}

// Launches a page to allow the user to download chrome.
// TODO(abodenha@chromium.org) Point to a custom page explaining what's wrong
// rather than the generic chrome download page.  See
// http://code.google.com/p/chromium/issues/detail?id=112019
void LaunchChromeDownloadPage() {
  if (kIsUnittest)
    return;
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    LOG(ERROR) << "Unable to get user token.";
    return;
  }
  base::win::ScopedHandle token_scoped(token);

  // Consider using the shell to invoke the default browser instead of hardcoded
  // reference to IE which might not be available on the system.
  base::FilePath ie_path;
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &ie_path);
  ie_path = ie_path.Append(kIePath);
  base::CommandLine command_line(ie_path);
  command_line.AppendArg(kChromeInstallUrl);

  base::LaunchOptions options;
  options.as_user = token_scoped.Get();
  base::LaunchProcess(command_line, options);
}

// Returns false if the print job is being run in a context
// that shouldn't be launching Chrome.
bool ValidateCurrentUser() {
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    // If we can't get the token we're probably not impersonating
    // the user, so validation should fail.
    return false;
  }
  base::win::ScopedHandle token_scoped(token);

  DWORD session_id = 0;
  DWORD dummy = 0;
  if (!::GetTokenInformation(token_scoped.Get(), TokenSessionId,
                             reinterpret_cast<void*>(&session_id),
                             sizeof(DWORD), &dummy)) {
    return false;
  }

  return session_id != 0;
}
}  // namespace

std::wstring ReadStringFromRegistry(HKEY root, const wchar_t* path_name) {
  base::win::RegKey gcp_key(root, kCloudPrintRegKey, KEY_READ);
  std::wstring data;
  gcp_key.ReadValue(path_name, &data);
  return data;
}

std::wstring ReadStringFromAnyRegistry(const wchar_t* path_name) {
  std::wstring result = ReadStringFromRegistry(HKEY_CURRENT_USER, path_name);
  if (!result.empty())
    return result;
  return ReadStringFromRegistry(HKEY_LOCAL_MACHINE, path_name);
}

base::FilePath GetChromeExePath() {
  std::wstring value = ReadStringFromAnyRegistry(kChromeExePathRegValue);
  if (!value.empty() && base::PathExists(base::FilePath(value)))
    return base::FilePath(value);
  return chrome_launcher_support::GetAnyChromePath(false /* is_sxs */);
}

base::FilePath GetChromeProfilePath() {
  std::wstring value = ReadStringFromAnyRegistry(kChromeProfilePathRegValue);
  if (!value.empty() && base::DirectoryExists(base::FilePath(value)))
    return base::FilePath(value);
  return base::FilePath();
}

// Launches the Cloud Print dialog in Chrome.
bool LaunchChromePrintDialog(const base::FilePath& xps_path,
                             const std::wstring& job_title) {
  base::FilePath chrome_path = GetChromeExePath();
  if (chrome_path.empty()) {
    LOG(ERROR) << "Unable to get chrome exe path.";
    LaunchChromeDownloadPage();
    return false;
  }

  base::CommandLine command_line(chrome_path);

  base::FilePath chrome_profile = GetChromeProfilePath();
  if (!chrome_profile.empty())
    command_line.AppendSwitchPath(switches::kUserDataDir, chrome_profile);

  command_line.AppendSwitchPath(switches::kCloudPrintFile, xps_path);
  command_line.AppendSwitchNative(switches::kCloudPrintFileType, kXpsMimeType);
  command_line.AppendSwitchNative(switches::kCloudPrintJobTitle, job_title);

  return LaunchCommandAsUser(command_line);
}

std::wstring GetPrintCommandTemplate() {
  return ReadStringFromAnyRegistry(kPrintCommandRegValue);
}

// Launches the print command. This will either launch Chrome to display the
// Cloud Print dialog or another exe as specified in the cloud print registry.
// xps_path references a file to print.
// job_title is the title to be used for the resulting print job.
bool LaunchPrintCommand(const base::FilePath& xps_path,
                        const std::wstring& job_title) {
  std::wstring command_template = GetPrintCommandTemplate();
  if (!command_template.empty()) {
    return LaunchPrintCommandFromTemplate(command_template, xps_path,
                                          job_title);
  } else {
    return LaunchChromePrintDialog(xps_path, job_title);
  }
}

BOOL WINAPI Monitor2EnumPorts(HANDLE,
                              wchar_t*,
                              DWORD level,
                              BYTE* ports,
                              DWORD ports_size,
                              DWORD* needed_bytes,
                              DWORD* returned) {
  if (needed_bytes == NULL) {
    LOG(ERROR) << "needed_bytes should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (level == 1) {
    *needed_bytes = sizeof(PORT_INFO_1);
  } else if (level == 2) {
    *needed_bytes = sizeof(PORT_INFO_2);
  } else {
    LOG(ERROR) << "Level " << level << "is not supported.";
    SetLastError(ERROR_INVALID_LEVEL);
    return FALSE;
  }
  *needed_bytes += static_cast<DWORD>(cloud_print::kPortNameSize);
  if (ports_size < *needed_bytes) {
    LOG(WARNING) << *needed_bytes << " bytes are required.  Only " << ports_size
                 << " were allocated.";
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  if (ports == NULL) {
    LOG(ERROR) << "ports should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (returned == NULL) {
    LOG(ERROR) << "returned should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  // Windows expects any strings refernced by PORT_INFO_X structures to
  // appear at the END of the buffer referenced by ports.  Placing
  // strings immediately after the PORT_INFO_X structure will cause
  // EnumPorts to fail until the spooler is restarted.
  // This is NOT mentioned in the documentation.
  wchar_t* string_target = reinterpret_cast<wchar_t*>(
      ports + ports_size - cloud_print::kPortNameSize);
  if (level == 1) {
    PORT_INFO_1* port_info = reinterpret_cast<PORT_INFO_1*>(ports);
    port_info->pName = string_target;
    StringCbCopy(port_info->pName, cloud_print::kPortNameSize,
                 cloud_print::kPortName);
  } else {
    PORT_INFO_2* port_info = reinterpret_cast<PORT_INFO_2*>(ports);
    port_info->pPortName = string_target;
    StringCbCopy(port_info->pPortName, cloud_print::kPortNameSize,
                 cloud_print::kPortName);
    port_info->pMonitorName = NULL;
    port_info->pDescription = NULL;
    port_info->fPortType = PORT_TYPE_WRITE;
    port_info->Reserved = 0;
  }
  *returned = 1;
  return TRUE;
}

BOOL WINAPI Monitor2OpenPort(HANDLE, wchar_t*, HANDLE* handle) {
  if (handle == NULL) {
    LOG(ERROR) << "handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  *handle = new PortData();
  return TRUE;
}

BOOL WINAPI Monitor2StartDocPort(HANDLE port_handle,
                                 wchar_t* printer_name,
                                 DWORD job_id,
                                 DWORD,
                                 BYTE*) {
  SetGoogleUpdateUsage(kGoogleUpdateProductId);
  if (port_handle == NULL) {
    LOG(ERROR) << "port_handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (printer_name == NULL) {
    LOG(ERROR) << "printer_name should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  port_data->job_id = job_id;
  if (!OpenPrinter(printer_name, &(port_data->printer_handle), NULL)) {
    LOG(WARNING) << "Unable to open printer " << printer_name << ".";
    // We can continue without a handle to the printer.
    // It just means we can't get the job title or tell the spooler that
    // the print job is complete.
    // This is the normal flow during a unit test.
    port_data->printer_handle = NULL;
  }
  base::FilePath& file_path = port_data->file_path;
  base::FilePath app_data_dir = GetAppDataDir();
  if (app_data_dir.empty())
    return FALSE;
  DeleteLeakedFiles(app_data_dir);
  if (!base::CreateDirectory(app_data_dir) ||
      !base::CreateTemporaryFileInDir(app_data_dir, &file_path)) {
    LOG(ERROR) << "Can't create temporary file in " << app_data_dir.value();
    return FALSE;
  }
  port_data->file = base::OpenFile(file_path, "wb+");
  if (port_data->file == nullptr) {
    LOG(ERROR) << "Error opening file " << file_path.value() << ".";
    return FALSE;
  }
  return TRUE;
}

BOOL WINAPI Monitor2WritePort(HANDLE port_handle,
                              BYTE* buffer,
                              DWORD buffer_size,
                              DWORD* bytes_written) {
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  *bytes_written =
      static_cast<DWORD>(fwrite(buffer, 1, buffer_size, port_data->file));
  if (*bytes_written > 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

BOOL WINAPI Monitor2ReadPort(HANDLE, BYTE*, DWORD, DWORD* read_bytes) {
  LOG(ERROR) << "Read is not supported.";
  *read_bytes = 0;
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}

BOOL WINAPI Monitor2EndDocPort(HANDLE port_handle) {
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  if (port_data == NULL) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (port_data->file != nullptr) {
    base::CloseFile(port_data->file);
    port_data->file = nullptr;
    bool delete_file = true;
    int64_t file_size = 0;
    base::GetFileSize(port_data->file_path, &file_size);
    if (file_size > 0) {
      std::wstring job_title;
      if (port_data->printer_handle != NULL) {
        GetJobTitle(port_data->printer_handle, port_data->job_id, &job_title);
      }
      if (LaunchPrintCommand(port_data->file_path, job_title)) {
        delete_file = false;
      }
    }
    if (delete_file)
      base::DeleteFile(port_data->file_path);
  }
  if (port_data->printer_handle != NULL) {
    // Tell the spooler that the job is complete.
    SetJob(port_data->printer_handle, port_data->job_id, 0, NULL,
           JOB_CONTROL_SENT_TO_PRINTER);
  }
  port_data->Close();
  // Return success even if we can't display the dialog.
  // TODO(abodenha@chromium.org) Come up with a better way of handling
  // this situation.
  return TRUE;
}

BOOL WINAPI Monitor2ClosePort(HANDLE port_handle) {
  if (port_handle == NULL) {
    LOG(ERROR) << "port_handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  delete reinterpret_cast<PortData*>(port_handle);
  return TRUE;
}

VOID WINAPI Monitor2Shutdown(HANDLE monitor_handle) {
  if (monitor_handle != NULL) {
    delete reinterpret_cast<MonitorData*>(monitor_handle);
  }
}

BOOL WINAPI Monitor2XcvOpenPort(HANDLE,
                                const wchar_t*,
                                ACCESS_MASK granted_access,
                                HANDLE* handle) {
  if (handle == NULL) {
    LOG(ERROR) << "handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  XcvUiData* xcv_data = new XcvUiData();
  xcv_data->granted_access = granted_access;
  *handle = xcv_data;
  return TRUE;
}

DWORD WINAPI Monitor2XcvDataPort(HANDLE xcv_handle,
                                 const wchar_t* data_name,
                                 BYTE*,
                                 DWORD,
                                 BYTE* output_data,
                                 DWORD output_data_bytes,
                                 DWORD* output_data_bytes_needed) {
  XcvUiData* xcv_data = reinterpret_cast<XcvUiData*>(xcv_handle);
  DWORD ret_val = ERROR_SUCCESS;
  if ((xcv_data->granted_access & SERVER_ACCESS_ADMINISTER) == 0) {
    return ERROR_ACCESS_DENIED;
  }
  if (output_data == NULL || output_data_bytes == 0) {
    return ERROR_INVALID_PARAMETER;
  }
  // We don't handle AddPort or DeletePort since we don't support
  // dynamic creation of ports.
  if (lstrcmp(L"MonitorUI", data_name) == 0) {
    DWORD dll_path_len = 0;
    base::FilePath dll_path(cloud_print::GetPortMonitorDllName());
    dll_path_len = static_cast<DWORD>(dll_path.value().length());
    if (output_data_bytes_needed != NULL) {
      *output_data_bytes_needed = dll_path_len;
    }
    if (output_data_bytes < dll_path_len) {
      return ERROR_INSUFFICIENT_BUFFER;
    } else {
      ret_val = StringCbCopy(reinterpret_cast<wchar_t*>(output_data),
                             output_data_bytes, dll_path.value().c_str());
    }
  } else {
    return ERROR_INVALID_PARAMETER;
  }
  return ret_val;
}

BOOL WINAPI Monitor2XcvClosePort(HANDLE handle) {
  delete reinterpret_cast<XcvUiData*>(handle);
  return TRUE;
}

BOOL WINAPI MonitorUiAddPortUi(const wchar_t*,
                               HWND hwnd,
                               const wchar_t* monitor_name,
                               wchar_t**) {
  HandlePortUi(hwnd, monitor_name);
  return TRUE;
}

BOOL WINAPI MonitorUiConfigureOrDeletePortUI(const wchar_t*,
                                             HWND hwnd,
                                             const wchar_t* port_name) {
  HandlePortUi(hwnd, port_name);
  return TRUE;
}

}  // namespace cloud_print

MONITOR2* WINAPI InitializePrintMonitor2(MONITORINIT*, HANDLE* handle) {
  if (handle == NULL) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
  }
  cloud_print::MonitorData* monitor_data = new cloud_print::MonitorData;
  *handle = monitor_data;
  if (!cloud_print::kIsUnittest) {
    // Unit tests set up their own AtExitManager
    monitor_data->at_exit_manager = std::make_unique<base::AtExitManager>();
    // Single spooler.exe handles verbose users.
    base::PathService::DisableCache();
  }
  return &cloud_print::g_monitor_2;
}

MONITORUI* WINAPI InitializePrintMonitorUI(void) {
  return &cloud_print::g_monitor_ui;
}
