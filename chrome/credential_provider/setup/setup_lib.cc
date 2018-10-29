// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/setup/setup_lib.h"

#include <shlobj.h>
#include <atlbase.h>
#include <iomanip>
#include <string>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

namespace {

constexpr base::FilePath::CharType kCredentialProviderDll[] =
    FILE_PATH_LITERAL("Gaia1_0.dll");

// List of files to install.  If the file list is changed here, make sure to
// update the files added in make_setup.py.
constexpr const base::FilePath::CharType* kFilenames[] = {
    FILE_PATH_LITERAL("gcp_setup.exe"),
    FILE_PATH_LITERAL("gcp_eventlog_provider.dll"),
    kCredentialProviderDll,  // Base name to the CP dll.
};

// List of dlls to register.  Must be a subset of kFilenames.
constexpr const base::FilePath::CharType* kRegsiterDlls[] = {
    kCredentialProviderDll,
};

// Creates the directory where GCP is to be installed.
base::FilePath CreateInstallDirectory() {
  base::FilePath dest_path;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES, &dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_PROGRAM_FILES) hr=" << putHR(hr);
    return base::FilePath();
  }

  dest_path = dest_path.Append(GetInstallParentDirectoryName())
                  .Append(FILE_PATH_LITERAL("Credential Provider"));

  if (!base::CreateDirectory(dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "base::CreateDirectory hr=" << putHR(hr);
    return base::FilePath();
  }

  return dest_path;
}

// Copies the files specified in |names| from |src_path| to |dest_path| creating
// any intermedidate subdirectories of |dest_path| as needed.  Both |src_path|
// and |dest_path| are full paths.  |names| are paths relative to |src_path|
// and are copied to the same relative path under |dest_path|.
HRESULT InstallFiles(const base::FilePath& src_path,
                     const base::FilePath& dest_path,
                     const base::FilePath::CharType* const names[],
                     size_t length) {
  for (size_t i = 0; i < length; ++i) {
    base::FilePath src = src_path.Append(names[i]);
    base::FilePath dest = dest_path.Append(names[i]);

    // Make sure parent of destination file exists.
    if (!base::CreateDirectory(dest.DirName())) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreateDirectory hr=" << putHR(hr)
                   << " name=" << names[i];
      return hr;
    }

    if (!base::CopyFile(src, dest)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CopyFile hr=" << putHR(hr) << " name=" << names[i];
      return hr;
    }
    LOGFN(INFO) << "Installed name=" << names[i];
  }

  return S_OK;
}

// Registers the named DLLs by calling the DllRegisterServer entrypoint.  The
// DLLs are specified with the |names| argument which are paths relative to
// |dest_path|.  |fakes| is non-null during unit tests to install fakes into
// the loaded DLL.
HRESULT RegisterDlls(const base::FilePath& dest_path,
                     const base::FilePath::CharType* const names[],
                     size_t length,
                     FakesForTesting* fakes) {
  bool has_failures = false;

  for (size_t i = 0; i < length; ++i) {
    base::ScopedNativeLibrary library(dest_path.Append(names[i]));

    if (fakes) {
      SetFakesForTestingFn set_fakes_for_testing_fn =
          reinterpret_cast<SetFakesForTestingFn>(
              library.GetFunctionPointer("SetFakesForTesting"));
      if (set_fakes_for_testing_fn)
        (*set_fakes_for_testing_fn)(fakes);
    }

    FARPROC register_server_fn = reinterpret_cast<FARPROC>(
        library.GetFunctionPointer("DllRegisterServer"));
    HRESULT hr = S_OK;

    if (register_server_fn) {
      hr = static_cast<HRESULT>((*register_server_fn)());
      LOGFN(INFO) << "Registered name=" << names[i] << " hr=" << putHR(hr);
    } else {
      LOGFN(ERROR) << "Failed to register name=" << names[i];
      hr = E_NOTIMPL;
    }
    has_failures |= FAILED(hr);
  }

  return has_failures ? E_UNEXPECTED : S_OK;
}

// Unregisters the named DLLs by calling the DllUneegisterServer entrypoint.
// The DLLs are specified with the |names| argument which are paths relative to
// |dest_path|.  |fakes| is non-null during unit tests to install fakes into
// the loaded DLL.
HRESULT UnregisterDlls(const base::FilePath& dest_path,
                       const base::FilePath::CharType* const names[],
                       size_t length,
                       FakesForTesting* fakes) {
  bool has_failures = false;

  for (size_t i = 0; i < length; ++i) {
    base::ScopedNativeLibrary library(dest_path.Append(names[i]));

    if (fakes) {
      SetFakesForTestingFn pmfn = reinterpret_cast<SetFakesForTestingFn>(
          library.GetFunctionPointer("SetFakesForTesting"));
      if (pmfn)
        (*pmfn)(fakes);
    }

    FARPROC pfn = reinterpret_cast<FARPROC>(
        library.GetFunctionPointer("DllUnregisterServer"));
    HRESULT hr = pfn ? static_cast<HRESULT>((*pfn)()) : E_UNEXPECTED;
    LOGFN(INFO) << "Unregistered name=" << names[i] << " hr=" << putHR(hr);
    has_failures |= FAILED(hr);
  }

  return has_failures ? E_UNEXPECTED : S_OK;
}

// Opens |path| with options that prevent the file from being read or written
// via another handle. As long as the returned object is alive, it is guaranteed
// that |path| isn't in use. It can however be deleted.
base::File GetFileLock(const base::FilePath& path) {
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_EXCLUSIVE_READ |
                              base::File::FLAG_EXCLUSIVE_WRITE |
                              base::File::FLAG_SHARE_DELETE);
}

// Deletes a specific GCP version from the disk.
void DeleteVersionDirectory(const base::FilePath& version_path) {
  // Lock all exes and dlls for exclusive access while allowing deletes.  Mark
  // the files for deletion and release them, causing them to actually be
  // deleted.  This allows the deletion of the version path itself.
  std::vector<base::File> locks;
  const int types = base::FileEnumerator::FILES;
  base::FileEnumerator enumerator_version(version_path, false, types,
                                          FILE_PATH_LITERAL("*"));
  bool all_deletes_succeeded = true;
  for (base::FilePath path = enumerator_version.Next(); !path.empty();
        path = enumerator_version.Next()) {
    if (!path.MatchesExtension(FILE_PATH_LITERAL(".exe")) &&
        !path.MatchesExtension(FILE_PATH_LITERAL(".dll"))) {
      continue;
    }

    // Open the file for exclusive access while allowing deletes.
    locks.push_back(GetFileLock(path));
    if (!locks.back().IsValid()) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "Could not lock " << path << " hr=" << putHR(hr);
      all_deletes_succeeded = false;
      continue;
    }

    // Mark the file for deletion.
    HRESULT hr = base::DeleteFile(path, false);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Could not delete " << path;
      all_deletes_succeeded = false;
    }
  }

  // Release the locks, actually deleting the files.  It is now possible to
  // delete the version path.
  locks.clear();
  if (all_deletes_succeeded && !base::DeleteFile(version_path, true))
    LOGFN(ERROR) << "Could not delete version " << version_path.BaseName();
}

// Deletes versions of GCP found under |gcp_path| except for version
// |product_version|.
//
// TODO(crbug.com/883935): figure out how to call this from credential provider
// code too. That way if older versions cannot be deleted at install time, they
// can eventually be cleaned up at next run.
void DeleteVersionsExcept(const base::FilePath& gcp_path,
                          const base::string16& product_version) {
  base::FilePath version = base::FilePath(product_version);
  const int types = base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator enumerator(gcp_path, false, types,
                                  FILE_PATH_LITERAL("*"));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    base::FilePath basename = name.BaseName();
    if (version == basename)
      continue;

    // Found an older version on the machine that can be deleted.  This is
    // best effort only.  If any errors occurred they are logged by
    // DeleteVersionDirectory().
    DeleteVersionDirectory(gcp_path.Append(basename));
  }
}

}  // namespace

namespace switches {

// These are command line switches to the setup program.

// Indicates the handle of the parent setup process when setup relaunches itself
// during uninstall.
const char kParentHandle[] = "parent-handle";

// Indicates the full path to the GCP installation to delete.  This switch is
// only used during uninstall.
const char kInstallPath[] = "install-path";

// Indicates to setup that it is being run to inunstall GCP.  If this switch
// is not present the assumption is to install GCP.
const char kUninstall[] = "uninstall";

}  // namespace switches

HRESULT DoInstall(const base::FilePath& installer_path,
                  const base::string16& product_version,
                  FakesForTesting* fakes) {
  const base::FilePath gcp_path = CreateInstallDirectory();
  if (gcp_path.empty())
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

  base::FilePath dest_path = gcp_path.Append(product_version);
  LOGFN(INFO) << "Install to: " << dest_path;

  // Make sure nothing under the destination directory is pending delete
  // after reboot, so that files installed now won't get deleted later.
  if (!RemoveFromMovesPendingReboot(dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "RemoveFromMovesPendingReboot hr=" << putHR(hr);
    return hr;
  }

  base::FilePath src_path = installer_path.DirName();
  HRESULT hr =
      InstallFiles(src_path, dest_path, kFilenames, base::size(kFilenames));
  if (FAILED(hr))
    return hr;

  hr = RegisterDlls(dest_path, kRegsiterDlls, base::size(kRegsiterDlls), fakes);
  if (FAILED(hr))
    return hr;

  // If all is good, try to delete all other versions on best effort basis.
  if (SUCCEEDED(hr))
    DeleteVersionsExcept(gcp_path, product_version);

  return S_OK;
}

HRESULT DoUninstall(const base::FilePath& installer_path,
                    const base::FilePath& dest_path,
                    FakesForTesting* fakes) {
  bool has_failures = false;

  // Do all actions best effort and keep going.
  has_failures |= FAILED(UnregisterDlls(dest_path, kRegsiterDlls,
                                        base::size(kRegsiterDlls), fakes));

  // Delete all files in the destination directory.  This directory does not
  // contain any configuration files or anything else user generated.
  if (!base::DeleteFile(dest_path, true)) {
    has_failures = true;
    ScheduleDirectoryForDeletion(dest_path);
  }

  // |dest_path| is of the form %ProgramFile%\Google\GCP\VERSION.  Now try to
  // delete the parent directory if possible.
  if (base::IsDirectoryEmpty(dest_path.DirName()))
    has_failures |= !base::DeleteFile(dest_path.DirName(), false);

  // TODO(rogerta): ask user to reboot if anything went wrong during uninstall.

  return has_failures ? E_UNEXPECTED : S_OK;
}

HRESULT RelaunchUninstaller(const base::FilePath& installer_path) {
  base::FilePath temp_path;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("gcp"), &temp_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateNewTempDirectory hr=" << putHR(hr);
    return hr;
  }

  base::FilePath new_installer_path =
      temp_path.Append(installer_path.BaseName());

  if (!base::CopyFile(installer_path, new_installer_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Could not copy uninstaller hr=" << putHR(hr) << " '"
                 << installer_path << "' --> '" << new_installer_path << "'";
    return hr;
  }

  base::win::ScopedHandle::Handle this_process_handle_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
                         GetCurrentProcess(), &this_process_handle_handle, 0,
                         TRUE,  // Inheritable.
                         DUPLICATE_SAME_ACCESS)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Error duplicating parent process handle.";
    return hr;
  }
  base::win::ScopedHandle this_process_handle(this_process_handle_handle);

  LOGFN(INFO) << "This process handle: " << this_process_handle_handle;

  base::CommandLine cmdline(new_installer_path);
  cmdline.AppendSwitch(switches::kUninstall);
  cmdline.AppendSwitchPath(switches::kInstallPath, installer_path.DirName());
  cmdline.AppendSwitchNative(switches::kParentHandle,
                             base::NumberToString16(base::win::HandleToUint32(
                                 this_process_handle_handle)));

  LOGFN(INFO) << "Cmd: " << cmdline.GetCommandLineString();

  base::LaunchOptions options;
  options.handles_to_inherit.push_back(this_process_handle_handle);
  options.current_directory = temp_path;
  base::Process process(base::LaunchProcess(cmdline, options));

  return process.IsValid() ? S_OK : E_FAIL;
}

void GetInstalledFileBasenames(const base::FilePath::CharType* const** names,
                               size_t* count) {
  *names = kFilenames;
  *count = base::size(kFilenames);
}

base::FilePath::StringType GetInstallParentDirectoryName() {
#if defined(GOOGLE_CHROME_BUILD)
  return FILE_PATH_LITERAL("Google");
#else
  return FILE_PATH_LITERAL("Chromium");
#endif
}

}  // namespace credential_provider
