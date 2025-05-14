// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/gcp_utils.h"

#include <string_view>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/process/launch.h"
#include "base/strings/strcat_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

TEST(GcpPasswordTest, GenerateRandomPassword) {
  wchar_t password[64];

  OSUserManager* manager = OSUserManager::Get();

  // Password buffer must be minimum length.
  ASSERT_NE(S_OK, manager->GenerateRandomPassword(password, 0));
  ASSERT_NE(S_OK, manager->GenerateRandomPassword(password, 23));

  // Generate a few passwords and make sure length i correct.
  for (int i = 0; i < 100; ++i) {
    ASSERT_EQ(S_OK,
              manager->GenerateRandomPassword(password, std::size(password)));
    ASSERT_LT(24u, wcslen(password));
  }
}

TEST(GcpOsVersionTest, GetOsFromRegistries) {
  registry_util::RegistryOverrideManager registry_override_;
  InitializeRegistryOverrideForTesting(&registry_override_);
  wchar_t kOsRegistryPath[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
  wchar_t kOsMajorName[] = L"CurrentMajorVersionNumber";
  wchar_t kOsMinorName[] = L"CurrentMinorVersionNumber";
  wchar_t kOsBuildName[] = L"CurrentBuildNumber";
  wchar_t kOsBuild[] = L"10819";
  DWORD major = 15;
  DWORD minor = 1;

  SetMachineRegDWORD(kOsRegistryPath, kOsMajorName, major);
  SetMachineRegDWORD(kOsRegistryPath, kOsMinorName, minor);
  SetMachineRegString(kOsRegistryPath, kOsBuildName, kOsBuild);

  std::string version;
  GetOsVersion(&version);

  ASSERT_EQ(version, "15.1.10819");
}

class GcpProcHelperTest : public ::testing::Test {
 protected:
  void CreateHandle(base::win::ScopedHandle* handle);

  bool TestPipe(const base::win::ScopedHandle::Handle& reading,
                const base::win::ScopedHandle::Handle& writing);

  void StripCrLf(char* buffer);

  FakeOSUserManager fake_os_user_manager_;
};

void GcpProcHelperTest::CreateHandle(base::win::ScopedHandle* handle) {
  handle->Set(
      ::CreateFileW(L"nul:", FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(handle->IsValid());
}

bool GcpProcHelperTest::TestPipe(
    const base::win::ScopedHandle::Handle& reading,
    const base::win::ScopedHandle::Handle& writing) {
  char input_buffer[8];
  char output_buffer[8];
  strcpy_s(input_buffer, std::size(input_buffer), "hello");
  const DWORD kExpectedDataLength = strlen(input_buffer) + 1;

  // Make sure what is written can be read.
  DWORD written;
  EXPECT_TRUE(::WriteFile(writing, input_buffer, kExpectedDataLength, &written,
                          nullptr));
  EXPECT_EQ(kExpectedDataLength, written);

  DWORD read;
  EXPECT_TRUE(ReadFile(reading, output_buffer, std::size(output_buffer), &read,
                       nullptr));
  EXPECT_EQ(kExpectedDataLength, read);
  return strcmp(input_buffer, output_buffer) == 0;
}

void GcpProcHelperTest::StripCrLf(char* buffer) {
  for (char* p = buffer + strlen(buffer) - 1; p >= buffer; --p) {
    if (*p == '\n' || *p == '\r')
      *p = 0;
  }
}

TEST_F(GcpProcHelperTest, ScopedStartupInfo) {
  ScopedStartupInfo info;
  ASSERT_EQ(sizeof(STARTUPINFOW), info.GetInfo()->cb);
  ASSERT_EQ(nullptr, info.GetInfo()->lpDesktop);
  ASSERT_EQ(0u, info.GetInfo()->dwFlags & STARTF_USESTDHANDLES);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdInput);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdOutput);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdError);
}

TEST_F(GcpProcHelperTest, ScopedStartupInfo_desktop) {
  ScopedStartupInfo info(L"desktop");
  ASSERT_EQ(sizeof(STARTUPINFOW), info.GetInfo()->cb);
  ASSERT_STREQ(L"desktop", info.GetInfo()->lpDesktop);
  ASSERT_EQ(0u, info.GetInfo()->dwFlags & STARTF_USESTDHANDLES);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdInput);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdOutput);
  ASSERT_EQ(INVALID_HANDLE_VALUE, info.GetInfo()->hStdError);
}

TEST_F(GcpProcHelperTest, ScopedStartupInfo_handles) {
  ScopedStartupInfo info;
  base::win::ScopedHandle shstdin;
  CreateHandle(&shstdin);
  base::win::ScopedHandle shstdout;
  CreateHandle(&shstdout);
  base::win::ScopedHandle shstderr;
  CreateHandle(&shstderr);

  // Setting handles in the info should take ownership.
  ASSERT_EQ(S_OK, info.SetStdHandles(&shstdin, &shstdout, &shstderr));
  ASSERT_FALSE(shstdin.IsValid());
  ASSERT_FALSE(shstdout.IsValid());
  ASSERT_FALSE(shstderr.IsValid());
  ASSERT_EQ(static_cast<DWORD>(STARTF_USESTDHANDLES),
            info.GetInfo()->dwFlags & STARTF_USESTDHANDLES);
  ASSERT_NE(INVALID_HANDLE_VALUE, info.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, info.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, info.GetInfo()->hStdError);
}

TEST_F(GcpProcHelperTest, ScopedStartupInfo_somehandles) {
  ScopedStartupInfo info;
  base::win::ScopedHandle shstdin;
  base::win::ScopedHandle shstdout;
  CreateHandle(&shstdout);
  base::win::ScopedHandle shstderr;

  // Setting handles in the info should take ownership.
  ASSERT_EQ(S_OK, info.SetStdHandles(&shstdin, &shstdout, &shstderr));
  ASSERT_FALSE(shstdin.IsValid());
  ASSERT_FALSE(shstdout.IsValid());
  ASSERT_FALSE(shstderr.IsValid());
  ASSERT_EQ(static_cast<DWORD>(STARTF_USESTDHANDLES),
            info.GetInfo()->dwFlags & STARTF_USESTDHANDLES);
  ASSERT_EQ(::GetStdHandle(STD_INPUT_HANDLE), info.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, info.GetInfo()->hStdOutput);
  ASSERT_EQ(::GetStdHandle(STD_ERROR_HANDLE), info.GetInfo()->hStdError);
}

TEST_F(GcpProcHelperTest, CreatePipeForChildProcess_ParentReads) {
  base::win::ScopedHandle reading;
  base::win::ScopedHandle writing;

  ASSERT_EQ(S_OK, CreatePipeForChildProcess(false, false, &reading, &writing));
  ASSERT_TRUE(reading.IsValid());
  ASSERT_TRUE(writing.IsValid());

  DWORD flags;
  ASSERT_TRUE(::GetHandleInformation(reading.Get(), &flags));
  ASSERT_EQ(0u, flags & HANDLE_FLAG_INHERIT);
  ASSERT_TRUE(::GetHandleInformation(writing.Get(), &flags));
  ASSERT_EQ(static_cast<DWORD>(HANDLE_FLAG_INHERIT),
            flags & HANDLE_FLAG_INHERIT);

  EXPECT_TRUE(TestPipe(reading.Get(), writing.Get()));
}

TEST_F(GcpProcHelperTest, CreatePipeForChildProcess_ChildReads) {
  base::win::ScopedHandle reading;
  base::win::ScopedHandle writing;

  ASSERT_EQ(S_OK, CreatePipeForChildProcess(true, false, &reading, &writing));
  ASSERT_TRUE(reading.IsValid());
  ASSERT_TRUE(writing.IsValid());

  DWORD flags;
  ASSERT_TRUE(::GetHandleInformation(reading.Get(), &flags));
  ASSERT_EQ(static_cast<DWORD>(HANDLE_FLAG_INHERIT),
            flags & HANDLE_FLAG_INHERIT);
  ASSERT_TRUE(::GetHandleInformation(writing.Get(), &flags));
  ASSERT_EQ(0u, flags & HANDLE_FLAG_INHERIT);

  EXPECT_TRUE(TestPipe(reading.Get(), writing.Get()));
}

TEST_F(GcpProcHelperTest, CreatePipeForChildProcess_ParentReadsNul) {
  base::win::ScopedHandle reading;
  base::win::ScopedHandle writing;

  ASSERT_EQ(S_OK, CreatePipeForChildProcess(false, true, &reading, &writing));
  ASSERT_FALSE(reading.IsValid());
  ASSERT_TRUE(writing.IsValid());  // Writes to nul:

  DWORD flags;
  ASSERT_TRUE(::GetHandleInformation(writing.Get(), &flags));
  ASSERT_EQ(static_cast<DWORD>(HANDLE_FLAG_INHERIT),
            flags & HANDLE_FLAG_INHERIT);
}

TEST_F(GcpProcHelperTest, CreatePipeForChildProcess_ChildReadsNul) {
  base::win::ScopedHandle reading;
  base::win::ScopedHandle writing;

  ASSERT_EQ(S_OK, CreatePipeForChildProcess(true, true, &reading, &writing));
  ASSERT_TRUE(reading.IsValid());  // Reads from nul:
  ASSERT_FALSE(writing.IsValid());

  DWORD flags;
  ASSERT_TRUE(::GetHandleInformation(reading.Get(), &flags));
  ASSERT_EQ(static_cast<DWORD>(HANDLE_FLAG_INHERIT),
            flags & HANDLE_FLAG_INHERIT);
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_ParentToChild) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK, InitializeStdHandles(CommDirection::kParentToChildOnly,
                                       kAllStdHandles, &startupinfo,
                                       &parent_handles));

  // Check parent handles.
  ASSERT_TRUE(parent_handles.hstdin_write.IsValid());
  ASSERT_FALSE(parent_handles.hstdout_read.IsValid());
  ASSERT_FALSE(parent_handles.hstderr_read.IsValid());

  // Check child handles.  stdout and stderr go to nul:.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdError);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(startupinfo.GetInfo()->hStdInput,
                       parent_handles.hstdin_write.Get()));
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_ChildToParent) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK, InitializeStdHandles(CommDirection::kChildToParentOnly,
                                       kAllStdHandles, &startupinfo,
                                       &parent_handles));

  // Check parent handles.
  ASSERT_FALSE(parent_handles.hstdin_write.IsValid());
  ASSERT_TRUE(parent_handles.hstdout_read.IsValid());
  ASSERT_TRUE(parent_handles.hstderr_read.IsValid());

  // Check child handles.  stdin comes from nul:.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdError);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(parent_handles.hstdout_read.Get(),
                       startupinfo.GetInfo()->hStdOutput));
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_ParentChildBirectional) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK,
            InitializeStdHandles(CommDirection::kBidirectional, kAllStdHandles,
                                 &startupinfo, &parent_handles));

  // Check parent handles.
  ASSERT_TRUE(parent_handles.hstdin_write.IsValid());
  ASSERT_TRUE(parent_handles.hstdout_read.IsValid());
  ASSERT_TRUE(parent_handles.hstderr_read.IsValid());

  // Check child handles.  stdin comes from nul:.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdError);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(startupinfo.GetInfo()->hStdInput,
                       parent_handles.hstdin_write.Get()));
  EXPECT_TRUE(TestPipe(parent_handles.hstdout_read.Get(),
                       startupinfo.GetInfo()->hStdOutput));
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_SomeHandlesChildToParent) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK, InitializeStdHandles(CommDirection::kChildToParentOnly,
                                       (kStdInput | kStdOutput), &startupinfo,
                                       &parent_handles));

  // Check parent handles.
  ASSERT_FALSE(parent_handles.hstdin_write.IsValid());
  ASSERT_TRUE(parent_handles.hstdout_read.IsValid());
  ASSERT_FALSE(parent_handles.hstderr_read.IsValid());

  // Check child handles. stderr goes to default handle.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_EQ(::GetStdHandle(STD_ERROR_HANDLE), startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(parent_handles.hstdout_read.Get(),
                       startupinfo.GetInfo()->hStdOutput));
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_SomeHandlesParentToChild) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK, InitializeStdHandles(CommDirection::kParentToChildOnly,
                                       (kStdInput | kStdOutput), &startupinfo,
                                       &parent_handles));

  // Check parent handles.
  ASSERT_TRUE(parent_handles.hstdin_write.IsValid());
  ASSERT_FALSE(parent_handles.hstdout_read.IsValid());
  ASSERT_FALSE(parent_handles.hstderr_read.IsValid());

  // Check child handles. stderr goes to default handle.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_EQ(::GetStdHandle(STD_ERROR_HANDLE), startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(startupinfo.GetInfo()->hStdInput,
                       parent_handles.hstdin_write.Get()));
}

TEST_F(GcpProcHelperTest, InitializeStdHandles_SomeHandlesBidirectional) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK, InitializeStdHandles(CommDirection::kBidirectional,
                                       (kStdInput | kStdOutput), &startupinfo,
                                       &parent_handles));

  // Check parent handles.
  ASSERT_TRUE(parent_handles.hstdin_write.IsValid());
  ASSERT_TRUE(parent_handles.hstdout_read.IsValid());
  ASSERT_FALSE(parent_handles.hstderr_read.IsValid());

  // Check child handles. stderr goes to default handle.
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdInput);
  ASSERT_NE(nullptr, startupinfo.GetInfo()->hStdOutput);
  ASSERT_NE(INVALID_HANDLE_VALUE, startupinfo.GetInfo()->hStdOutput);
  ASSERT_EQ(::GetStdHandle(STD_ERROR_HANDLE), startupinfo.GetInfo()->hStdError);

  EXPECT_TRUE(TestPipe(startupinfo.GetInfo()->hStdInput,
                       parent_handles.hstdin_write.Get()));
  EXPECT_TRUE(TestPipe(parent_handles.hstdout_read.Get(),
                       startupinfo.GetInfo()->hStdOutput));
}

TEST_F(GcpProcHelperTest, WaitForProcess) {
  ScopedStartupInfo startupinfo;
  StdParentHandles parent_handles;

  ASSERT_EQ(S_OK,
            InitializeStdHandles(CommDirection::kBidirectional, kAllStdHandles,
                                 &startupinfo, &parent_handles));
  base::LaunchOptions options;
  options.inherit_mode = base::LaunchOptions::Inherit::kAll;
  options.stdin_handle = startupinfo.GetInfo()->hStdInput;
  options.stdout_handle = startupinfo.GetInfo()->hStdOutput;
  options.stderr_handle = startupinfo.GetInfo()->hStdError;

  // Launch an app that copies stdin to stdout.  This is not a perfect copy
  // of linux cat since it appends \r\n to the output strings.  The test
  // needs to take that in account below.
  base::Process process(base::LaunchProcess(L"find.exe /v \"\"", options));
  ASSERT_TRUE(process.IsValid());

  // Write to stdin of the child process.
  const int kBufferSize = 16;
  char input_buffer[kBufferSize];
  strcpy_s(input_buffer, std::size(input_buffer), "hello");
  const DWORD kExpectedDataLength = strlen(input_buffer) + 1;
  DWORD written;
  ASSERT_TRUE(::WriteFile(parent_handles.hstdin_write.Get(), input_buffer,
                          kExpectedDataLength, &written, nullptr));
  ASSERT_EQ(kExpectedDataLength, written);
  ASSERT_TRUE(FlushFileBuffers(parent_handles.hstdin_write.Get()));
  parent_handles.hstdin_write.Close();

  //  Close all child handles that the parent is still holding onto, to ensure
  // the child process quits.  Otherwise the pipe will remain open and the child
  // will continue to wait for input.
  startupinfo.Shutdown();

  DWORD exit_code;
  char output_buffer[kBufferSize];
  EXPECT_EQ(S_OK, WaitForProcess(process.Handle(), parent_handles, &exit_code,
                                 output_buffer, kBufferSize));
  EXPECT_EQ(0u, exit_code);
  StripCrLf(output_buffer);
  EXPECT_STREQ(input_buffer, output_buffer);
}

TEST_F(GcpProcHelperTest, GetCommandLineForEntrypoint) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  // In tests, GetCommandLineForEntrypoint() will always return S_FALSE.
  ASSERT_EQ(S_FALSE,
            GetCommandLineForEntrypoint(nullptr, L"entrypoint", &command_line));

  // Get short path name of this binary and build the expect command line.
  wchar_t path[MAX_PATH];
  wchar_t short_path[MAX_PATH];
  ASSERT_LT(0u, GetModuleFileName(nullptr, path, std::size(path)));
  ASSERT_LT(0u, GetShortPathName(path, short_path, std::size(short_path)));

  std::wstring expected_arg =
      base::StrCat({L"\"", short_path, L"\",entrypoint"});

  ASSERT_EQ(1u, command_line.GetArgs().size());
  ASSERT_EQ(expected_arg, command_line.GetArgs()[0]);
}

TEST(Enroll, EnrollToGoogleMdmIfNeeded_NotEnabled) {
  // Make sure MDM is not enforced.
  registry_util::RegistryOverrideManager registry_override;
  InitializeRegistryOverrideForTesting(&registry_override);

  // EnrollToGoogleMdmIfNeeded() should be a noop.
  ASSERT_EQ(S_OK, EnrollToGoogleMdmIfNeeded(base::Value::Dict()));
}

// Tests all possible data combinations sent to EnrollToGoogleMdmIfNeeded to
// ensure correct error reporting is performed.
// Parameters are:
// 1. Email.
// 2. Id token.
// 3. Access token.
// 4. User SID.
// 5. Username.
// 6. Domain.
// 7. Serial Number.
// 8. Machine Guid.
// 9. Is ADJoined User.
// 10 Device resource Id.
class GcpEnrollmentArgsTest
    : public ::testing::TestWithParam<std::tuple<const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*,
                                                 const char*>> {};

TEST_P(GcpEnrollmentArgsTest, EnrollToGoogleMdmIfNeeded_MissingArgs) {
  // Enforce successful MDM enrollment. We just want to verify that correct
  // verification of the dictionary is being performed in the function.
  registry_util::RegistryOverrideManager registry_override;
  InitializeRegistryOverrideForTesting(&registry_override);
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  GoogleMdmEnrollmentStatusForTesting forced_enrollment_status(true);
  GoogleMdmEnrolledStatusForTesting forced_enrolled_status(false);

  const char* email = std::get<0>(GetParam());
  const char* id_token = std::get<1>(GetParam());
  const char* access_token = std::get<2>(GetParam());
  const char* sid = std::get<3>(GetParam());
  const char* username = std::get<4>(GetParam());
  const char* domain = std::get<5>(GetParam());
  const char* serial_number = std::get<6>(GetParam());
  const char* machine_guid = std::get<7>(GetParam());
  const char* is_user_ad_joined = std::get<8>(GetParam());
  const char* device_resource_id = std::get<9>(GetParam());
  FakeOSUserManager fake_os_user_manager;

  const auto has = [](const char* param) { return param && param[0]; };
  bool should_succeed = has(email) && has(id_token) && has(access_token) &&
                        has(sid) && has(username) && has(domain) &&
                        has(serial_number) && has(machine_guid) &&
                        has(is_user_ad_joined);

  base::Value::Dict properties;
  const auto set_property = [&](std::string_view key, const char* value) {
    if (value) {
      properties.Set(key, value);
    }
  };
  set_property(kKeyEmail, email);
  set_property(kKeyMdmIdToken, id_token);
  set_property(kKeyAccessToken, access_token);
  set_property(kKeySID, sid);
  set_property(kKeyUsername, username);
  set_property(kKeyDomain, domain);
  GoogleRegistrationDataForTesting g_registration_data(
      base::UTF8ToWide(serial_number));
  SetMachineGuidForTesting(base::UTF8ToWide(machine_guid));
  set_property(kKeyIsAdJoinedUser, is_user_ad_joined);
  EXPECT_TRUE(
      SUCCEEDED(SetUserProperty(base::UTF8ToWide(sid), kRegUserDeviceResourceId,
                                base::UTF8ToWide(device_resource_id))));

  // EnrollToGoogleMdmIfNeeded() should fail if any field is missing.
  EXPECT_EQ(should_succeed, EnrollToGoogleMdmIfNeeded(properties) == S_OK);
}

INSTANTIATE_TEST_SUITE_P(
    GcpRegistrationData,
    GcpEnrollmentArgsTest,
    ::testing::Combine(::testing::Values("foo@gmail.com", "", nullptr),
                       ::testing::Values("id_token", "", nullptr),
                       ::testing::Values("access_token", "", nullptr),
                       ::testing::Values("sid", ""),
                       ::testing::Values("username", "", nullptr),
                       ::testing::Values("domain", "", nullptr),
                       ::testing::Values("serial_number"),
                       ::testing::Values("machine_guid"),
                       ::testing::Values("true", "false"),
                       ::testing::Values("device_resource_id", "")));

INSTANTIATE_TEST_SUITE_P(
    GcpRegistrationHardwareIds,
    GcpEnrollmentArgsTest,
    ::testing::Combine(::testing::Values("foo@gmail.com"),
                       ::testing::Values("id_token"),
                       ::testing::Values("access_token"),
                       ::testing::Values("sid"),
                       ::testing::Values("username"),
                       ::testing::Values("domain"),
                       ::testing::Values("serial_number", ""),
                       ::testing::Values("machine_guid", ""),
                       ::testing::Values("true"),
                       ::testing::Values("device_resource_id", "")));

}  // namespace credential_provider
