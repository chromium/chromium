// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wininet.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"
#include "components/services/quarantine/quarantine.h"
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

  return base::WriteFile(file_path, kTestData, base::size(kTestData)) ==
         static_cast<int>(base::size(kTestData));
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

  ScopedZoneForSite(const base::string16& domain,
                    const base::string16& protocol,
                    ZoneIdentifierType zone_identifier_type);
  ~ScopedZoneForSite();

 private:
  base::string16 domain_;
  base::string16 protocol_;

  DISALLOW_COPY_AND_ASSIGN(ScopedZoneForSite);
};

ScopedZoneForSite::ScopedZoneForSite(const base::string16& domain,
                                     const base::string16& protocol,
                                     ZoneIdentifierType zone_identifier_type)
    : domain_(domain), protocol_(protocol) {
  base::string16 registry_path = base::StringPrintf(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet "
      L"Settings\\ZoneMap\\Domains\\%ls",
      domain_.c_str());
  base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_path.c_str(),
                                 KEY_SET_VALUE);

  EXPECT_EQ(registry_key.WriteValue(protocol_.c_str(), zone_identifier_type),
            ERROR_SUCCESS);
}

ScopedZoneForSite::~ScopedZoneForSite() {
  base::string16 registry_path = base::StringPrintf(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet "
      L"Settings\\ZoneMap\\Domains\\%ls",
      domain_.c_str());
  base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_path.c_str(),
                                 KEY_SET_VALUE);
  registry_key.DeleteValue(protocol_.c_str());
}

// Sets the internet Zone.Identifier alternate data stream for |file_path|.
bool AddInternetZoneIdentifierDirectly(const base::FilePath& file_path) {
  static const char kMotwForInternetZone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
  return base::WriteFile(GetZoneIdentifierStreamPath(file_path),
                         kMotwForInternetZone,
                         base::size(kMotwForInternetZone)) ==
         static_cast<int>(base::size(kMotwForInternetZone));
}

}  // namespace

class QuarantineWinTest : public ::testing::Test {
 public:
  QuarantineWinTest() = default;
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

  const wchar_t* GetTrustedSite() { return L"thisisatrustedsite.com"; }

  const wchar_t* GetRestrictedSite() { return L"thisisarestrictedsite.com"; }

  const wchar_t* GetInternetSite() { return L"example.com"; }

 private:
  registry_util::RegistryOverrideManager registry_override_;

  base::ScopedTempDir scoped_temp_dir_;

  // Due to caching, these sites zone must be set for all tests, so that the
  // order the tests are run does not matter.
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_trusted_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_internet_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_restricted_site_;

  DISALLOW_COPY_AND_ASSIGN(QuarantineWinTest);
};

// If the file is missing, the QuarantineFile() call should return FILE_MISSING.
TEST_F(QuarantineWinTest, MissingFile) {
  EXPECT_EQ(QuarantineFileResult::FILE_MISSING,
            QuarantineFile(GetTempDir().AppendASCII("does-not-exist.exe"),
                           GURL(kDummySourceUrl), GURL(kDummyReferrerUrl),
                           kDummyClientGuid));
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

    EXPECT_EQ(
        QuarantineFileResult::OK,
        QuarantineFile(test_file, GURL(source_url), GURL(), kDummyClientGuid));

    std::string zone_identifier;
    GetZoneIdentifierStreamContents(test_file, &zone_identifier);

    // No zone identifier for local source.
    EXPECT_TRUE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
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
    EXPECT_EQ(
        QuarantineFileResult::OK,
        QuarantineFile(test_file, GURL(source_url), GURL(), kDummyClientGuid));

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
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

  for (const auto referrer_url : unsafe_referrers) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << referrer_url);

    ASSERT_TRUE(CreateFile(test_file));
    EXPECT_EQ(QuarantineFileResult::OK,
              QuarantineFile(test_file, GURL("http://example.com/good"),
                             GURL(referrer_url), kDummyClientGuid));

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
}

// An empty source URL should result in a file that's treated the same as one
// downloaded from the internet.
TEST_F(QuarantineWinTest, EmptySource_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(), GURL(), kDummyClientGuid));

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
  ASSERT_EQ(0, base::WriteFile(test_file, "", 0u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           kDummyClientGuid));

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

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           std::string()));

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
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(source_url), GURL(), std::string()));

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
  GURL source_url = GURL(
      base::StringPrintf(L"https://%ls/folder/good.exe", GetTrustedSite()));

  ASSERT_TRUE(CreateFile(test_file));
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, source_url, GURL(), kDummyClientGuid));

  // No zone identifier.
  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, RestrictedSite) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("bad.exe");
  GURL source_url = GURL(
      base::StringPrintf(L"https://%ls/folder/bad.exe", GetRestrictedSite()));

  ASSERT_TRUE(CreateFile(test_file));

  // Files from a restricted site are deleted.
  EXPECT_EQ(QuarantineFileResult::BLOCKED_BY_POLICY,
            QuarantineFile(test_file, source_url, GURL(), kDummyClientGuid));

  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, TrustedSite_AlreadyQuarantined) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("good.exe");
  GURL source_url = GURL(
      base::StringPrintf(L"https://%ls/folder/good.exe", GetTrustedSite()));

  ASSERT_TRUE(CreateFile(test_file));
  // Ensure the file already contains a zone identifier.
  ASSERT_TRUE(AddInternetZoneIdentifierDirectly(test_file));
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, source_url, GURL(), kDummyClientGuid));

  // The existing zone identifier was not removed.
  std::string zone_identifier;
  EXPECT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_FALSE(zone_identifier.empty());
}

TEST_F(QuarantineWinTest, RestrictedSite_AlreadyQuarantined) {
  // Test file path and source URL.
  base::FilePath test_file = GetTempDir().AppendASCII("bad.exe");
  GURL source_url = GURL(
      base::StringPrintf(L"https://%ls/folder/bad.exe", GetRestrictedSite()));

  ASSERT_TRUE(CreateFile(test_file));
  // Ensure the file already contains a zone identifier.
  ASSERT_TRUE(AddInternetZoneIdentifierDirectly(test_file));

  // Files from a restricted site are deleted.
  EXPECT_EQ(QuarantineFileResult::BLOCKED_BY_POLICY,
            QuarantineFile(test_file, source_url, GURL(), kDummyClientGuid));

  std::string zone_identifier;
  EXPECT_FALSE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));
}

TEST_F(QuarantineWinTest, MetaData_ApplyMOTW_Directly) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  GURL host_url = GURL(base::StringPrintf(
      L"https://user:pass@%ls/folder/foo.exe?x#y", GetInternetSite()));
  GURL host_url_clean = GURL(
      base::StringPrintf(L"https://%ls/folder/foo.exe?x#y", GetInternetSite()));
  GURL referrer_url = GURL(base::StringPrintf(
      L"https://user:pass@%ls/folder/index?x#y", GetInternetSite()));
  GURL referrer_url_clean = GURL(
      base::StringPrintf(L"https://%ls/folder/index?x#y", GetInternetSite()));

  // An invalid GUID will cause QuarantineFile() to apply the MOTW directly.
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, host_url, referrer_url, std::string()));

  EXPECT_TRUE(IsFileQuarantined(test_file, host_url_clean, referrer_url_clean));
}

TEST_F(QuarantineWinTest, MetaData_InvokeAS) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  GURL host_url = GURL(
      base::StringPrintf(L"https://%ls/folder/foo.exe?x#y", GetInternetSite()));
  GURL host_url_clean = GURL(
      base::StringPrintf(L"https://%ls/folder/foo.exe?x#y", GetInternetSite()));
  GURL referrer_url = GURL(base::StringPrintf(
      L"https://user:pass@%ls/folder/index?x#y", GetInternetSite()));
  GURL referrer_url_clean = GURL(
      base::StringPrintf(L"https://%ls/folder/index?x#y", GetInternetSite()));

  EXPECT_EQ(
      QuarantineFileResult::OK,
      QuarantineFile(test_file, host_url, referrer_url, kDummyClientGuid));

  EXPECT_TRUE(IsFileQuarantined(test_file, host_url_clean, referrer_url_clean));
}

}  // namespace quarantine
