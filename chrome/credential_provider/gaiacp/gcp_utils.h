// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_

#include <windows.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"
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
class FilePath;

}  // namespace base

namespace credential_provider {

// Windows supports a maximum of 20 characters plus null in username.
inline constexpr int kWindowsUsernameBufferLength = 21;

inline constexpr int kWindowsPasswordBufferLength = 32;

// Maximum domain length is 256 characters including null.
// https://support.microsoft.com/en-ca/help/909264/naming-conventions-in-active-directory-for-computers-domains-sites-and
constexpr int kWindowsDomainBufferLength = 256;

// According to:
// https://stackoverflow.com/questions/1140528/what-is-the-maximum-length-of-a-sid-in-sddl-format
constexpr int kWindowsSidBufferLength = 184;

// Max number of attempts to find a new username when a user already exists
// with the same username.
constexpr int kMaxUsernameAttempts = 10;

// First index to append to a username when another user with the same name
// already exists.
constexpr int kInitialDuplicateUsernameIndex = 2;

// Default extension used as a fallback if the picture_url returned from gaia
// does not have a file extension.
extern const wchar_t kDefaultProfilePictureFileExtension[];

// Name of the sub-folder under which all files for GCPW are stored.
extern const base::FilePath::CharType kCredentialProviderFolder[];

// Default URL for the GEM MDM API.
extern const wchar_t kDefaultMdmUrl[];

// Maximum number of consecutive Upload device details failures for which we do
// enforce auth.
extern const int kMaxNumConsecutiveUploadDeviceFailures;

// Maximum allowed time delta after which user policies should be refreshed
// again.
extern const base::TimeDelta kMaxTimeDeltaSinceLastUserPolicyRefresh;

// Maximum allowed time delta after which experiments should be fetched
// again.
extern const base::TimeDelta kMaxTimeDeltaSinceLastExperimentsFetch;

// Path elements for the path where the experiments are stored on disk.
extern const wchar_t kGcpwExperimentsDirectory[];
extern const wchar_t kGcpwUserExperimentsFileName[];

// Because of some strange dependency problems with windows header files,
// define STATUS_SUCCESS here instead of including ntstatus.h or SubAuth.h
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

// A bitfield indicating which standard handles are to be created.
using StdHandlesToCreate = uint32_t;

enum : uint32_t {
  kStdOutput = 1 << 0,
  kStdInput = 1 << 1,
  kStdError = 1 << 2,
  kAllStdHandles = kStdOutput | kStdInput | kStdError
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

// Class used in tests to set registration data for testing.
class GoogleRegistrationDataForTesting {
 public:
  explicit GoogleRegistrationDataForTesting(std::wstring serial_number);
  ~GoogleRegistrationDataForTesting();
};

// Class used in tests to set gem device details for testing.
class GemDeviceDetailsForTesting {
 public:
  explicit GemDeviceDetailsForTesting(std::vector<std::string>& mac_addresses,
                                      std::string os_version);
  ~GemDeviceDetailsForTesting();
};

// Class used in tests to set chrome path for testing.
class GoogleChromePathForTesting {
 public:
  explicit GoogleChromePathForTesting(base::FilePath chrome_path);
  ~GoogleChromePathForTesting();
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
  std::wstring desktop_;
};

// Gets the brand specific path in which to install GCPW.
base::FilePath::StringType GetInstallParentDirectoryName();

// Gets the directory where the GCP is installed
base::FilePath GetInstallDirectory();

// Deletes versions of GCP found under |gcp_path| except for version
// |product_version|.
void DeleteVersionsExcept(const base::FilePath& gcp_path,
                          const std::wstring& product_version);

// Waits for the process specified by |procinfo| to terminate.  The handles
// in |read_handles| can be used to read stdout/err from the process.  Upon
// return, |exit_code| contains one of the UIEC_xxx constants listed above,
// and |stdout_buffer| and |stderr_buffer| contain the output, if any.
// Both buffers must be at least |buffer_size| characters long.
HRESULT WaitForProcess(base::win::ScopedHandle::Handle process_handle,
                       const StdParentHandles& parent_handles,
                       DWORD* exit_code,
                       char* output_buffer,
                       int buffer_size);

// Creates a restricted, batch or interactive login token for the given user.
HRESULT CreateLogonToken(const wchar_t* domain,
                         const wchar_t* username,
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
                             StdHandlesToCreate to_create,
                             ScopedStartupInfo* startupinfo,
                             StdParentHandles* parent_handles);

// Fills |path_to_dll| with the short path to the dll referenced by
// |dll_handle|. The short path is needed to correctly call rundll32.exe in
// cases where there might be quotes or spaces in the path.
HRESULT GetPathToDllFromHandle(HINSTANCE dll_handle,
                               base::FilePath* path_to_dll);

// This function gets a correctly formatted entry point argument to pass to
// rundll32.exe for a dll referenced by the handle |dll_handle| and an entry
// point function with the name |entrypoint|. |entrypoint_arg| will be filled
// with the argument value.
HRESULT GetEntryPointArgumentForRunDll(HINSTANCE dll_handle,
                                       const wchar_t* entrypoint,
                                       std::wstring* entrypoint_arg);

// This function is used to build the command line for rundll32 to call an
// exported entrypoint from the DLL given by |dll_handle|.
// Returns S_FALSE if a command line can successfully be built but if the
// path to the "dll" actually points to a non ".dll" file. This allows
// detection of calls to this function via a unit test which will be
// running under an ".exe" module.
HRESULT GetCommandLineForEntrypoint(HINSTANCE dll_handle,
                                    const wchar_t* entrypoint,
                                    base::CommandLine* command_line);

// Looks up the name associated to the |sid| (if any). Returns an error on any
// failure or no name is associated with the |sid|.
HRESULT LookupLocalizedNameBySid(PSID sid, std::wstring* localized_name);

// Gets localalized name for builtin administrator account.
HRESULT GetLocalizedNameBuiltinAdministratorAccount(
    std::wstring* builtin_localized_admin_name);

// Looks up the name associated to the well known |sid_type| (if any). Returns
// an error on any failure or no name is associated with the |sid_type|.
HRESULT LookupLocalizedNameForWellKnownSid(WELL_KNOWN_SID_TYPE sid_type,
                                           std::wstring* localized_name);

// Handles the writing and deletion of a startup sentinel file used to ensure
// that the GCPW does not crash continuously on startup and render the
// winlogon process unusable.
bool WriteToStartupSentinel();
void DeleteStartupSentinel();
void DeleteStartupSentinelForVersion(const std::wstring& version);

// Gets a string resource from the DLL with the given id.
std::wstring GetStringResource(UINT base_message_id);

// Gets a string resource from the DLL with the given id after replacing the
// placeholders with the provided substitutions.
std::wstring GetStringResource(UINT base_message_id,
                               const std::vector<std::wstring>& subst);

// Gets the language selected by the base::win::i18n::LanguageSelector.
std::wstring GetSelectedLanguage();

// Securely clear a base::Value::Dict that may have a password field.
void SecurelyClearDictionaryValue(base::optional_ref<base::Value::Dict> dict);
void SecurelyClearDictionaryValueWithKey(
    base::optional_ref<base::Value::Dict> dict,
    const std::string& password_key);

// Securely clear std::wstring and std::string.
void SecurelyClearString(std::wstring& str);
void SecurelyClearString(std::string& str);

// Securely clear a given |buffer| with size |length|.
void SecurelyClearBuffer(void* buffer, size_t length);

// Helpers to get strings from base::Value::Dict.
std::wstring GetDictString(const base::Value::Dict& dict, const char* name);
std::string GetDictStringUTF8(const base::Value::Dict& dict, const char* name);

// Perform a recursive search on a nested dictionary object. Note that the
// names provided in the input should be in order. Below is an example : Lets
// say the json object is {"key1": {"key2": {"key3": "value1"}}, "key4":
// "value2"}. Then to search for the key "key3", this method should be called
// by providing the |path| as {"key1", "key2", "key3"}.
std::string SearchForKeyInStringDictUTF8(
    const std::string& json_string,
    const std::initializer_list<std::string_view>& path);

// Perform a recursive search on a nested dictionary object. Note that the
// names provided in the input should be in order. Below is an example : Lets
// say the json object is
// {"key1": {"key2": {"value": "value1", "value": "value2"}}}.
// Then to search for the key "key2" and list_key as "value", then this method
// should be called by providing |list_key| as "value", |path| as
// ["key1", "key2"] and the result returned would be ["value1", "value2"].
HRESULT SearchForListInStringDictUTF8(
    const std::string& list_key,
    const std::string& json_string,
    const std::initializer_list<std::string_view>& path,
    std::vector<std::string>* output);

// Returns the major build version of Windows by reading the registry.
// See:
// https://stackoverflow.com/questions/31072543/reliable-way-to-get-windows-version-from-registry
std::wstring GetWindowsVersion();

// Returns the minimum supported version of Chrome for GCPW.
base::Version GetMinimumSupportedChromeVersion();

class OSUserManager;
class OSProcessManager;

// This structure is used in tests to set fake objects in the credential
// provider dll.  See the function SetFakesForTesting() for details.
struct FakesForTesting {
  FakesForTesting();
  ~FakesForTesting();

  ScopedLsaPolicy::CreatorCallback scoped_lsa_policy_creator;
  raw_ptr<OSUserManager> os_user_manager_for_testing = nullptr;
  raw_ptr<OSProcessManager> os_process_manager_for_testing = nullptr;
  WinHttpUrlFetcher::CreatorCallback fake_win_http_url_fetcher_creator;
  raw_ptr<InternetAvailabilityChecker>
      internet_availability_checker_for_testing = nullptr;
};

// DLL entrypoint signature for settings testing fakes.  This is used by
// the setup tests to install fakes into the dynamically loaded gaia1_0 DLL
// static data.  This way the production DLL does not need to include binary
// code used only for testing.
typedef void CALLBACK (*SetFakesForTestingFn)(const FakesForTesting* fakes);

// Initializes the members of a Windows STRING struct (UNICODE_STRING or
// LSA_STRING) to point to the string pointed to by |string|.
template <class WindowsStringT,
          class WindowsStringCharT = decltype(WindowsStringT().Buffer[0])>
void InitWindowsStringWithString(const WindowsStringCharT* string,
                                 WindowsStringT* windows_string) {
  constexpr size_t buffer_char_size = sizeof(WindowsStringCharT);
  windows_string->Buffer = const_cast<WindowsStringCharT*>(string);
  windows_string->Length = static_cast<USHORT>(
      std::char_traits<WindowsStringCharT>::length((windows_string->Buffer)) *
      buffer_char_size);
  windows_string->MaximumLength = windows_string->Length + buffer_char_size;
}

// Extracts the provided keys from the given dictionary. Returns true if all
// keys are found. If any of the key isn't found, returns false.
bool ExtractKeysFromDict(
    const base::Value::Dict& dict,
    const std::vector<std::pair<std::string, std::string*>>& needed_outputs);

// Gets the bios serial number of the windows device.
std::wstring GetSerialNumber();

// Gets the mac addresses of the windows device.
std::vector<std::string> GetMacAddresses();

// Gets the OS version installed on the device. The format is
// "major.minor.build".
void GetOsVersion(std::string* version);

// Gets the obfuscated device_id that is a combination of multiple device
// identifiers.
HRESULT GenerateDeviceId(std::string* device_id);

// Overrides the gaia_url and gcpw_endpoint_path that is used to load GLS.
HRESULT SetGaiaEndpointCommandLineIfNeeded(const wchar_t* override_registry_key,
                                           const std::string& default_endpoint,
                                           bool provide_deviceid,
                                           bool show_tos,
                                           base::CommandLine* command_line);

// Returns the file path to installed chrome.exe.
base::FilePath GetChromePath();

// Returns the file path to system installed chrome.exe.
base::FilePath GetSystemChromePath();

// Generates gcpw dm token for the given |sid|. If any of the lsa operations
// fail, function returns a result other than S_OK.
HRESULT GenerateGCPWDmToken(const std::wstring& sid);

// Reads the gcpw dm token from lsa store for the given |sid| and writes it back
// in |token| output parameter.  If any of the lsa operations fail, function
// returns a result other than S_OK.
HRESULT GetGCPWDmToken(const std::wstring& sid, std::wstring* token);

// Gets the gcpw service URL.
GURL GetGcpwServiceUrl();

// Converts the |url| in the form of http://xxxxx.googleapis.com/...
// to a form that points to a development URL as specified with |dev|
// environment. Final url will be in the form
// https://{dev}-xxxxx.sandbox.googleapis.com/...
std::wstring GetDevelopmentUrl(const std::wstring& url,
                               const std::wstring& dev);

// Returns a handle to a file which is stored under DIR_COMMON_APP_DATA > |sid|
// > |file_dir| > |file_name|. The file is opened with the provided
// |open_flags|.
std::unique_ptr<base::File> GetOpenedFileForUser(const std::wstring& sid,
                                                 uint32_t open_flags,
                                                 const std::wstring& file_dir,
                                                 const std::wstring& file_name);

// Returns the time delta since the last fetch for the given |sid|. |flag|
// stores the last fetch time.
base::TimeDelta GetTimeDeltaSinceLastFetch(const std::wstring& sid,
                                           const std::wstring& flag);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_
