// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wrl/implements.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"
#include "chrome/windows_services/service_program/test_support/scoped_mock_context.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elevation_service {

namespace {

// There is no exe in "valid_publisher.crx3", so we point to the manifest
// instead for these tests.
constexpr base::FilePath::CharType kManifestJSONFileName[] =
    FILE_PATH_LITERAL("manifest.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign "valid_publisher.crx3",
// which houses extension id ojjgnpkioondelmggbekfhllhdaimnho.
std::vector<uint8_t> GetValidPublisherCrx3Hash() {
  return std::vector<uint8_t>{0xe9, 0x96, 0xdf, 0xa8, 0xee, 0xd3, 0x4b, 0xc6,
                              0x61, 0x4a, 0x57, 0xbb, 0x73, 0x08, 0xcd, 0x7e,
                              0x51, 0x9b, 0xcc, 0x69, 0x08, 0x41, 0xe1, 0x96,
                              0x9f, 0x7c, 0xb1, 0x73, 0xef, 0x16, 0x80, 0x0a};
}

// The test exe within components/test/data/update_client/ChromeRecovery.crx3.
constexpr base::FilePath::CharType kRecoveryExeName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

// The SHA256 of the SubjectPublicKeyInfo used to sign
// components/test/data/update_client/ChromeRecovery.crx3.
std::vector<uint8_t> GetRunactionTestWinCrx3Hash() {
  return std::vector<uint8_t>{0x69, 0xfc, 0x41, 0xf6, 0x17, 0x20, 0xc6, 0x36,
                              0x92, 0xcd, 0x95, 0x76, 0x69, 0xf6, 0x28, 0xcc,
                              0xbe, 0x98, 0x4b, 0x93, 0x17, 0xd6, 0x9c, 0xb3,
                              0x64, 0x0c, 0x0d, 0x25, 0x61, 0xc5, 0x80, 0x1d};
}

const base::FilePath GetUnpackDir() {
  base::FilePath path;
  base::PathService::Get(base::DIR_TEMP, &path);
  return path;
}

const base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  return path.AppendASCII("elevated_recovery_unittest")
      .AppendASCII(file);
}

}  // namespace

class ElevatedRecoveryTest : public testing::Test {
 protected:
  ElevatedRecoveryTest() = default;

  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());
    ASSERT_TRUE(mock_context_.Succeeded());
  }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
  ScopedMockContext mock_context_;
};

TEST_F(ElevatedRecoveryTest, Do_RunChromeRecoveryCRX_InvalidArgs) {
  base::win::ScopedHandle proc_handle;

  // Empty browser_appid/browser_version/session_id.
  EXPECT_EQ(E_INVALIDARG, elevation_service::RunChromeRecoveryCRX(
                              TestFile("valid_publisher.crx3"), std::wstring(),
                              std::wstring(), std::wstring(),
                              ::GetCurrentProcessId(), &proc_handle));

  // Invalid browser_appid, valid browser_version/session_id.
  EXPECT_EQ(E_INVALIDARG,
            elevation_service::RunChromeRecoveryCRX(
                TestFile("valid_publisher.crx3"), L"invalidappid", L"1.2.3.4",
                L"{c49ab053-2387-4809-b188-1902648802e1}",
                ::GetCurrentProcessId(), &proc_handle));

  // Empty browser_appid, invalid browser_version, valid session_id.
  EXPECT_EQ(E_INVALIDARG, elevation_service::RunChromeRecoveryCRX(
                              TestFile("valid_publisher.crx3"), std::wstring(),
                              L"invalidbrowserversion",
                              L"{c49ab053-2387-4809-b188-1902648802e1}",
                              ::GetCurrentProcessId(), &proc_handle));

  // Valid browser_appid, invalid browser_version, valid session_id.
  EXPECT_EQ(E_INVALIDARG, elevation_service::RunChromeRecoveryCRX(
                              TestFile("valid_publisher.crx3"),
                              L"{c49ab053-2387-4809-b188-1902648802e1}",
                              L"invalidbrowserversion",
                              L"{c49ab053-2387-4809-b188-1902648802e1}",
                              ::GetCurrentProcessId(), &proc_handle));

  // Valid browser_appid, valid browser_version, invalid session_id.
  EXPECT_EQ(E_INVALIDARG,
            elevation_service::RunChromeRecoveryCRX(
                TestFile("valid_publisher.crx3"),
                L"{c49ab053-2387-4809-b188-1902648802e1}", L"57.8.0.1",
                L"invalidsessionid", ::GetCurrentProcessId(), &proc_handle));
}

TEST_F(ElevatedRecoveryTest, Do_RunCRX_InvalidArgs) {
  base::win::ScopedHandle proc_handle;

  // Non-matching CRX/CRX-hash.
  EXPECT_EQ(CRYPT_E_NO_MATCH,
            elevation_service::RunCRX(
                TestFile("valid_no_publisher.crx3"),
                base::CommandLine(base::CommandLine::NO_PROGRAM),
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
                GetValidPublisherCrx3Hash(), GetUnpackDir(),
                base::FilePath(kManifestJSONFileName), ::GetCurrentProcessId(),
                &proc_handle));

  // Non-existent CRX file.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            elevation_service::RunCRX(
                TestFile("nonexistent.crx3"),
                base::CommandLine(base::CommandLine::NO_PROGRAM),
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
                GetValidPublisherCrx3Hash(), GetUnpackDir(),
                base::FilePath(kManifestJSONFileName), ::GetCurrentProcessId(),
                &proc_handle));

  // manifest.json is not a Windows executable, ::CreateProcess therefore
  // returns ERROR_BAD_EXE_FORMAT.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT),
            elevation_service::RunCRX(
                TestFile("valid_publisher.crx3"),
                base::CommandLine(base::CommandLine::NO_PROGRAM),
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
                GetValidPublisherCrx3Hash(), GetUnpackDir(),
                base::FilePath(kManifestJSONFileName), ::GetCurrentProcessId(),
                &proc_handle));
}

TEST_F(ElevatedRecoveryTest, Do_RunCRX_ValidArgs) {
  base::win::ScopedHandle proc_handle;

  // ChromeRecovery.crx3 contains ChromeRecovery.exe which returns a hardcoded
  // value of 1877345072.
  EXPECT_EQ(S_OK,
            elevation_service::RunCRX(
                TestFile("ChromeRecovery.crx3"),
                base::CommandLine(base::CommandLine::NO_PROGRAM),
                crx_file::VerifierFormat::CRX3, GetRunactionTestWinCrx3Hash(),
                GetUnpackDir(), base::FilePath(kRecoveryExeName),
                ::GetCurrentProcessId(), &proc_handle));

  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(proc_handle.Get(), 500));
  DWORD exit_code = 0;
  EXPECT_TRUE(::GetExitCodeProcess(proc_handle.Get(), &exit_code));
  EXPECT_EQ(1877345072UL, exit_code);
}

TEST(ElevatedRecoveryCleanupTest, Do_CleanupChromeRecoveryDirectory) {
  base::FilePath recovery_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &recovery_dir));
  recovery_dir = recovery_dir.DirName().DirName().Append(
      FILE_PATH_LITERAL("ChromeRecovery"));
  ASSERT_TRUE(base::CreateDirectory(recovery_dir));

  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(recovery_dir, &temp_file));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(recovery_dir, &temp_file));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(recovery_dir, &temp_file));

  base::ScopedTempDir scoped_dir;
  ASSERT_TRUE(scoped_dir.CreateUniqueTempDirUnderPath(recovery_dir));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(scoped_dir.GetPath(), &temp_file));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(scoped_dir.GetPath(), &temp_file));

  EXPECT_EQ(S_OK, elevation_service::CleanupChromeRecoveryDirectory());

  EXPECT_TRUE(base::PathExists(recovery_dir));
  EXPECT_TRUE(base::IsDirectoryEmpty(recovery_dir));
}

}  // namespace elevation_service
