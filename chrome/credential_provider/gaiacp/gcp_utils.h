// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "url/gurl.h"

// These define are documented in
// https://msdn.microsoft.com/en-us/library/bb470234(v=vs.85).aspx not available
// in the user mode headers.
#define DIRECTORY_QUERY 0x00000001
#define DIRECTORY_TRAVERSE 0x00000002
#define DIRECTORY_CREATE_OBJECT 0x00000004
#define DIRECTORY_CREATE_SUBDIRECTORY 0x00000008
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)

namespace base {

class CommandLine;

}  // namespace base

namespace credential_provider {

// Because of some strange dependency problems with windows header files,
// define STATUS_SUCCESS here instead of including ntstatus.h or SubAuth.h
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

// The the UI process can exit with the following exit code.
enum UiExitCodes {
  // The user completed the sign in successfully.
  kUiecSuccess,

  // The sign in was aborted by the user.
  kUiecAbort,

  // The sign in timed out.
  kUiecTimeout,

  // The process was killed by the GCP.
  kUiecKilled,

  // The email does not match the required pattern.
  kUiecEMailMissmatch,
};

// Filled in by InitializeStdHandles to return the parent side of stdin/stdout/
// stderr pipes of the login UI process.
struct StdParentHandles {
  StdParentHandles();
  ~StdParentHandles();

  base::win::ScopedHandle hstdin_write;
  base::win::ScopedHandle hstdout_read;
  base::win::ScopedHandle hstderr_read;
};

// Process startup options that allows customization of stdin/stdout/stderr
// handles.
class ScopedStartupInfo {
 public:
  ScopedStartupInfo();
  explicit ScopedStartupInfo(const wchar_t* desktop);
  ~ScopedStartupInfo();

  // This function takes ownership of the handles.
  HRESULT SetStdHandles(base::win::ScopedHandle* hstdin,
                        base::win::ScopedHandle* hstdout,
                        base::win::ScopedHandle* hstderr);

  LPSTARTUPINFOW GetInfo() { return &info_; }

  // Releases all resources held by this info.
  void Shutdown();

 private:
  STARTUPINFOW info_;
  base::string16 desktop_;
};

// Waits for the process specified by |procinfo| to terminate.  The handles
// in |read_handles| can be used to read stdout/err from the process.  Upon
// return, |exit_code| contains one of the UIEC_xxx constants listed above,
// and |stdout_buffer| and |stderr_buffer| contain the output, if any.
// Both buffers must be at least |buffer_size| characters long.
HRESULT WaitForProcess(base::win::ScopedHandle::Handle process_handle,
                       const StdParentHandles& parent_handles,
                       DWORD* exit_code,
                       char* stdout_buffer,
                       char* stderr_buffer,
                       int buffer_size);

// Creates a restricted, batch or interactive login token for the given user.
HRESULT CreateLogonToken(const wchar_t* username,
                         const wchar_t* password,
                         bool interactive,
                         base::win::ScopedHandle* token);

HRESULT CreateJobForSignin(base::win::ScopedHandle* job);

// Creates a pipe that can be used by a parent process to communicate with a
// child process.  If |child_reads| is false, then it is expected that the
// parent process will read from |reading| anything the child process writes
// to |writing|.  For example, this is used to read stdout/stderr of child.
//
// If |child_reads| is true, then it is expected that the child process will
// read from |reading| anything the parent process writes to |writing|.  For
// example, this is used to write to stdin of child.
//
// If |use_nul| is true, then the parent's handle is not used (can be passed
// as nullptr).  The child reads from or writes to the null device.
HRESULT CreatePipeForChildProcess(bool child_reads,
                                  bool use_nul,
                                  base::win::ScopedHandle* reading,
                                  base::win::ScopedHandle* writing);

// Initializes 3 pipes for communicating with a child process.  On return,
// |startupinfo| will be set with the handles needed by the child.  This is
// used when creating the child process.  |parent_handles| contains the
// corresponding handles to be used by the parent process.
//
// Communication direction is used to optimize handle creation.  If
// communication occurs in only one direction then some pipes will be directed
// to the nul device.
enum class CommDirection {
  kParentToChildOnly,
  kChildToParentOnly,
  kBidirectional,
};
HRESULT InitializeStdHandles(CommDirection direction,
                             ScopedStartupInfo* startupinfo,
                             StdParentHandles* parent_handles);

// This function is used to build the command line for rundll32 to call an
// exported entrypoint from the DLL given by |hDll|.
HRESULT GetCommandLineForEntrypoint(HINSTANCE hDll,
                                    const wchar_t* entrypoint,
                                    base::CommandLine* command_line);

// Enrolls the machine to with the Google MDM server if not already.
HRESULT EnrollToGoogleMdmIfNeeded(const base::DictionaryValue& properties);

// Gets the auth package id for NEGOSSP_NAME_A.
HRESULT GetAuthenticationPackageId(ULONG* id);

// Gets a string resource from the DLL with the given id.
base::string16 GetStringResource(UINT id);

// Helpers to get strings from DictionaryValues.
base::string16 GetDictString(const base::DictionaryValue* dict,
                             const char* name);
base::string16 GetDictString(const std::unique_ptr<base::DictionaryValue>& dict,
                             const char* name);
std::string GetDictStringUTF8(const base::DictionaryValue* dict,
                              const char* name);
std::string GetDictStringUTF8(
  const std::unique_ptr<base::DictionaryValue>& dict,
  const char* name);

class OSUserManager;

// This structure is used in tests to set fake objects in the credential
// provider dll.  See the function SetFakesForTesting() for details.
struct FakesForTesting {
  FakesForTesting();
  ~FakesForTesting();

  ScopedLsaPolicy::CreatorCallback scoped_lsa_policy_creator;
  OSUserManager* os_manager_for_testing = nullptr;
};

// DLL entrypoint signature for settings testing fakes.  This is used by
// the setup tests to install fakes into the dynamically loaded gaia1_0 DLL
// static data.  This way the production DLL does not need to include binary
// code used only for testing.
typedef void CALLBACK (*SetFakesForTestingFn)(const FakesForTesting* fakes);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_
