// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <windows.h>

#include <wininet.h>

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/services/quarantine/test_support.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace quarantine {

namespace {

const char kDummySourceUrl[] = "https://example.com/foo";
const char kDummyReferrerUrl[] = "https://example.com/referrer";
const char kDummyClientGuid[] = "A1B69307-8FA2-4B6F-9181-EA06051A48A7";

const char* const kUntrustedURLs[] = {
    "http://example.com/foo",
    "https://example.com/foo",
    "ftp://example.com/foo",
    "ftp://example.com:2121/foo",
    "data:text/plain,Hello%20world",
    "blob:https://example.com/126278b3-58f3-4b4a-a914-1d1185d634f6",
    "about:internet",
    ""};

// Creates a non-empty file at |file_path|.
bool CreateFile(const base::FilePath& file_path) {
  constexpr char kTestData[] = "Hello world!";

  return base::WriteFile(file_path, kTestData);
}

base::FilePath GetZoneIdentifierStreamPath(const base::FilePath& file_path) {
  const base::FilePath::CharType kMotwStreamSuffix[] =
      FILE_PATH_LITERAL(":Zone.Identifier");

  return base::FilePath(file_path.value() + kMotwStreamSuffix);
}

// Reads the Zone.Identifier alternate data stream from |file_path| into
// |contents|.
bool GetZoneIdentifierStreamContents(const base::FilePath& file_path,
                                     std::string* contents) {
  DCHECK(contents);
  return base::ReadFileToString(GetZoneIdentifierStreamPath(file_path),
                                contents);
}

// Maps a domain and protocol to a zone.
class ScopedZoneForSite {
 public:
  enum ZoneIdentifierType : DWORD {
    kMyComputer = 0,
    kLocalIntranetZone = 1,
    kTrustedSitesZone = 2,
    kInternetZone = 3,
    kRestrictedSitesZone = 4,
  };

  ScopedZoneForSite(std::string_view domain,
                    std::wstring_view protocol,
                    ZoneIdentifierType zone_identifier_type);

  ScopedZoneForSite(const ScopedZoneForSite&) = delete;
  ScopedZoneForSite& operator=(const ScopedZoneForSite&) = delete;

  ~ScopedZoneForSite();

 private:
  std::wstring GetRegistryPath() const;

  std::wstring domain_;
  std::wstring protocol_;
};

ScopedZoneForSite::ScopedZoneForSite(std::string_view domain,
                                     std::wstring_view protocol,
                                     ZoneIdentifierType zone_identifier_type)
    : domain_(base::ASCIIToWide(domain)), protocol_(protocol) {
  base::win::RegKey registry_key(HKEY_CURRENT_USER, GetRegistryPath().c_str(),
                                 KEY_SET_VALUE);

  EXPECT_EQ(registry_key.WriteValue(protocol_.c_str(), zone_identifier_type),
            ERROR_SUCCESS);
}

ScopedZoneForSite::~ScopedZoneForSite() {
  base::win::RegKey registry_key(HKEY_CURRENT_USER, GetRegistryPath().c_str(),
                                 KEY_SET_VALUE);
  registry_key.DeleteValue(protocol_.c_str());
}

std::wstring ScopedZoneForSite::GetRegistryPath() const {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet "
         L"Settings\\ZoneMap\\Domains\\" +
         domain_;
}

// Sets the internet Zone.Identifier alternate data stream for |file_path|.
bool AddInternetZoneIdentifierDirectly(const base::FilePath& file_path) {
  static const char kMotwForInternetZone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
  return base::WriteFile(GetZoneIdentifierStreamPath(file_path),
                         kMotwForInternetZone);
}

void CheckQuarantineResult(QuarantineFileResult result,
                           QuarantineFileResult expected_result) {
  EXPECT_EQ(expected_result, result);
}

}  // namespace

class QuarantineWinTest : public ::testing::Test {
 public:
  QuarantineWinTest() = default;

  QuarantineWinTest(const QuarantineWinTest&) = delete;
  QuarantineWinTest& operator=(const QuarantineWinTest&) = delete;

  ~QuarantineWinTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    scoped_zone_for_trusted_site_ = std::make_unique<ScopedZoneForSite>(
        GetTrustedSite(), L"https",
        ScopedZoneForSite::ZoneIdentifierType::kTrustedSitesZone);
    scoped_zone_for_restricted_site_ = std::make_unique<ScopedZoneForSite>(
        GetRestrictedSite(), L"https",
        ScopedZoneForSite::ZoneIdentifierType::kRestrictedSitesZone);
    scoped_zone_for_internet_site_ = std::make_unique<ScopedZoneForSite>(
        GetInternetSite(), L"https",
        ScopedZoneForSite::ZoneIdentifierType::kInternetZone);
  }

  base::FilePath GetTempDir() { return scoped_temp_dir_.GetPath(); }

  std::string_view GetTrustedSite() { return "thisisatrustedsite.com"; }

  std::string_view GetRestrictedSite() { return "thisisarestrictedsite.com"; }

  std::string_view GetInternetSite() { return "example.com"; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  registry_util::RegistryOverrideManager registry_override_;

  base::ScopedTempDir scoped_temp_dir_;

  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::Uninitialization::kBlockPremature};

  // Due to caching, these sites zone must be set for all tests, so that the
  // order the tests are run does not matter.
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_trusted_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_internet_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_restricted_site_;
};

// If the file is missing, the QuarantineFile() call should return FILE_MISSING.
TEST_F(QuarantineWinTest, MissingFile) {
  QuarantineFile(GetTempDir().AppendASCII("does-not-exist.exe"),
                 GURL(kDummySourceUrl), GURL(kDummyReferrerUrl),
                 /*request_initiator=*/std::nullopt, kDummyClientGuid,
                 base::BindOnce(&CheckQuarantineResult,
                                QuarantineFileResult::FILE_MISSING));
  base::RunLoop().RunUntilIdle();
}

// On Windows systems, files downloaded from a local source are considered
// trustworthy. Hence they aren't annotated with source information. This test
// verifies this behavior since the other tests in this suite would pass with a
// false positive if local files are being annotated with the MOTW for the
// internet zone.
TEST_F(QuarantineWinTest, LocalFile_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  const char* const kLocalSourceURLs[] = {"http://localhost/foo",
                                          "file:///C:/some-local-dir/foo.exe"};

  for (const char* source_url : kLocalSourceURLs) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << source_url);

    ASSERT_TRUE(CreateFile(test_file));

    QuarantineFile(
        test_file, GURL(source_url), GURL(), /*request_initiator=*/std::nullopt,
        kDummyClientGuid,
        base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
    base::RunLoop().RunUntilIdle();

    std::string zone_identifier;
    GetZoneIdentifierStreamContents(test_file, &zone_identifier);

    // No zone identifier for local source.
    EXPECT_TRUE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file));
  }
}

// A file downloaded from the internet should be annotated with .. something.
// The specific zone assigned to our dummy source URL depends on the local
// configuration. But no sane configuration should be treating the dummy URL as
// a trusted source for anything.
TEST_F(QuarantineWinTest, DownloadedFile_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  for (const char* source_url : kUntrustedURLs) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << source_url);

    ASSERT_TRUE(CreateFile(test_file));

    QuarantineFile(
        test_file, GURL(source_url), GURL(), /*request_initiator=*/std::nullopt,
        kDummyClientGuid,
        base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
    base::RunLoop().RunUntilIdle();

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file));
  }
}

TEST_F(QuarantineWinTest, UnsafeReferrer_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  std::vector<std::string> unsafe_referrers(std::begin(kUntrustedURLs),
                                            std::end(kUntrustedURLs));

  // Add one more test case.
  std::string huge_referrer = "http://example.com/";
  huge_referrer.append(INTERNET_MAX_URL_LENGTH * 2, 'a');
  unsafe_referrers.push_back(huge_referrer);

  for (const auto& referrer_url : unsafe_referrers) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << referrer_url);

    ASSERT_TRUE(CreateFile(test_file));
    QuarantineFile(
        test_file, GURL("http://example.com/good"), GURL(referrer_url),
        /*request_initiator=*/std::nullopt, kDummyClientGuid,
        base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
    base::RunLoop().RunUntilIdle();

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file));
  }
}

// An empty source URL should result in a file that's treated the same as one
// downloaded from the internet.
TEST_F(QuarantineWinTest, EmptySource_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  QuarantineFile(
      test_file, GURL(), GURL(), /*request_initiator=*/std::nullopt,
      kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  // The actual assigned zone could be anything and the contents of the zone
  // identifier depends on the version of Windows. So only testing that there is
  // a zone annotation.
  EXPECT_FALSE(zone_identifier.empty());
}

// Empty files aren't passed to AVScanFile. They are instead marked manually. If
// the file is passed to AVScanFile, then there wouldn't be a MOTW attached to
// it and the test would fail.
TEST_F(QuarantineWinTest, EmptyFile) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(base::WriteFile(test_file, ""));

  QuarantineFile(
      test_file, net::FilePathToFileURL(test_file), GURL(),
      /*request_initiator=*/std::nullopt, kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  // The actual assigned zone could be anything and the contents of the zone
  // identifier depends on the version of Windows. So only testing that there is
  // a zone annotation.
  EXPECT_FALSE(zone_identifier.empty());
}

// If there is no client GUID supplied to the QuarantineFile() call, then rather
// than invoking AVScanFile, the MOTW will be applied manually.  If the file is
// passed to AVScanFile, then there wouldn't be a MOTW attached to it and the
// test would fail.
TEST_F(QuarantineWinTest, NoClientGuid) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  QuarantineFile(
      test_file, net::FilePathToFileURL(test_file), GURL(),
      /*request_initiator=*/std::nullopt, std::string(),
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  // The actual assigned zone could be anything and the contents of the zone
  // identifier depends on the version of Windows. So only testing that there is
  // a zone annotation.
  EXPECT_FALSE(zone_identifier.empty());
}

// URLs longer than INTERNET_MAX_URL_LENGTH are known to break URLMon. Such a
// URL, when used as a source URL shouldn't break QuarantineFile() which should
// mark the file as being from the internet zone as a safe fallback.
TEST_F(QuarantineWinTest, SuperLongURL) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  std::string source_url("http://example.com/");
  source_url.append(INTERNET_MAX_URL_LENGTH * 2, 'a');
  QuarantineFile(
      test_file, GURL(source_url), GURL(), /*request_initiator=*/std::nullopt,
      std::string(),
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  // The actual assigned zone could be anything and the contents of the zone
  // identifier depends on the version of Windows. So only testing that there is
  // a zone annotation.
  EXPECT_FALSE(zone_identifier.empty());
}

TEST_F(QuarantineWinTest, TrustedSite) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("good.exe");
  GURL source_url(
      base::StrCat({"https://", GetTrustedSite(), "/folder/good.exe"}));

  ASSERT_TRUE(CreateFile(test_file));
  QuarantineFile(
      test_file, source_url, GURL(), /*request_initiator=*/std::nullopt,
      kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  // No zone identifier.
  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, RestrictedSite) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("bad.exe");
  GURL source_url(
      base::StrCat({"https://", GetRestrictedSite(), "/folder/bad.exe"}));

  ASSERT_TRUE(CreateFile(test_file));

  // Files from a restricted site are deleted.
  QuarantineFile(test_file, source_url, GURL(),
                 /*request_initiator=*/std::nullopt, kDummyClientGuid,
                 base::BindOnce(&CheckQuarantineResult,
                                QuarantineFileResult::BLOCKED_BY_POLICY));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, TrustedSite_AlreadyQuarantined) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("good.exe");
  GURL source_url(
      base::StrCat({"https://", GetTrustedSite(), "/folder/good.exe"}));

  ASSERT_TRUE(CreateFile(test_file));
  // Ensure the file already contains a zone identifier.
  ASSERT_TRUE(AddInternetZoneIdentifierDirectly(test_file));
  QuarantineFile(
      test_file, source_url, GURL(), /*request_initiator=*/std::nullopt,
      kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  // The existing zone identifier was not removed.
  std::string zone_identifier;
  EXPECT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_FALSE(zone_identifier.empty());
}

TEST_F(QuarantineWinTest, RestrictedSite_AlreadyQuarantined) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("bad.exe");
  GURL source_url(
      base::StrCat({"https://", GetRestrictedSite(), "/folder/bad.exe"}));

  ASSERT_TRUE(CreateFile(test_file));
  // Ensure the file already contains a zone identifier.
  ASSERT_TRUE(AddInternetZoneIdentifierDirectly(test_file));

  // Files from a restricted site are deleted.
  QuarantineFile(test_file, source_url, GURL(),
                 /*request_initiator=*/std::nullopt, kDummyClientGuid,
                 base::BindOnce(&CheckQuarantineResult,
                                QuarantineFileResult::BLOCKED_BY_POLICY));
  base::RunLoop().RunUntilIdle();

  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, MetaData_ApplyMOTW_Directly) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  GURL host_url(base::StrCat(
      {"https://user:pass@", GetInternetSite(), "/folder/foo.exe?x#y"}));
  GURL host_url_clean(
      base::StrCat({"https://", GetInternetSite(), "/folder/foo.exe?x#y"}));
  GURL referrer_url(base::StrCat(
      {"https://user:pass@", GetInternetSite(), "/folder/index?x#y"}));
  GURL referrer_url_clean(
      base::StrCat({"https://", GetInternetSite(), "/folder/index?x#y"}));

  // An invalid GUID will cause QuarantineFile() to apply the MOTW directly.
  QuarantineFile(
      test_file, host_url, referrer_url, /*request_initiator=*/std::nullopt,
      std::string(),
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsFileQuarantined(test_file, host_url_clean, referrer_url_clean));
}

TEST_F(QuarantineWinTest, MetaData_InvokeAS) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  GURL host_url(base::StrCat(
      {"https://user:pass@", GetInternetSite(), "/folder/foo.exe?x#y"}));
  GURL host_url_clean(
      base::StrCat({"https://", GetInternetSite(), "/folder/foo.exe?x#y"}));
  GURL referrer_url(base::StrCat(
      {"https://user:pass@", GetInternetSite(), "/folder/index?x#y"}));
  GURL referrer_url_clean(
      base::StrCat({"https://", GetInternetSite(), "/folder/index?x#y"}));

  QuarantineFile(
      test_file, host_url, referrer_url, /*request_initiator=*/std::nullopt,
      kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsFileQuarantined(test_file, host_url_clean, referrer_url_clean));
}

TEST_F(QuarantineWinTest, RequestInitiatorReplacesSourceUrl) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  GURL host_url(base::StrCat({"https://", GetInternetSite(), "/"}));
  QuarantineFile(
      test_file, GURL("data://text/html,payload"), GURL(),
      url::Origin::Create(host_url), kDummyClientGuid,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsFileQuarantined(test_file, host_url, GURL()));
}

}  // namespace quarantine
