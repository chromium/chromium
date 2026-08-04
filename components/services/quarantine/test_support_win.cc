// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/test_support.h"

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_win.h"

namespace quarantine {

// Static helper to set up domain zone mapping in HKCU
class ScopedZoneForSite {
 public:
  enum class ZoneIdentifierType {
    kLocalIntranetZone = 1,
    kTrustedSitesZone = 2,
    kInternetZone = 3,
    kRestrictedSitesZone = 4,
  };

  ScopedZoneForSite(std::string_view host,
                    std::string_view scheme,
                    ZoneIdentifierType zone)
      : host_(host), scheme_(scheme) {
    SetZone(zone);
  }

  ScopedZoneForSite(const ScopedZoneForSite&) = delete;
  ScopedZoneForSite& operator=(const ScopedZoneForSite&) = delete;

  ~ScopedZoneForSite() { ResetZone(); }

 private:
  void SetZone(ZoneIdentifierType zone) {
    base::FilePath zone_map_key(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet "
        L"Settings\\ZoneMap\\Domains");
    base::FilePath host_key = zone_map_key.AppendASCII(host_);
    base::win::RegKey key;
    if (key.Create(HKEY_CURRENT_USER, host_key.value().c_str(),
                   KEY_ALL_ACCESS) == ERROR_SUCCESS) {
      key.WriteValue(base::ASCIIToWide(scheme_).c_str(),
                     static_cast<DWORD>(zone));
    }
  }

  void ResetZone() {
    base::FilePath zone_map_key(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet "
        L"Settings\\ZoneMap\\Domains");
    base::FilePath host_key = zone_map_key.AppendASCII(host_);
    base::win::RegKey key;
    if (key.Open(HKEY_CURRENT_USER, host_key.value().c_str(), KEY_ALL_ACCESS) ==
        ERROR_SUCCESS) {
      key.DeleteValue(base::ASCIIToWide(scheme_).c_str());
    }
  }

  std::string host_;
  std::string scheme_;
};

QuarantineTestBase::QuarantineTestBase() = default;
QuarantineTestBase::~QuarantineTestBase() = default;

std::string_view QuarantineTestBase::GetTrustedSite() {
  return "thisisatrustedsite.com";
}

std::string_view QuarantineTestBase::GetRestrictedSite() {
  return "thisisarestrictedsite.com";
}

std::string_view QuarantineTestBase::GetInternetSite() {
  return "example.com";
}

std::string_view QuarantineTestBase::GetLocalIntranetSite() {
  return "localhost";
}

// Both QuarantineTest and QuarantineTestWin can trigger loading of urlmon.dll.
// urlmon.dll caches zone scopes, so overriding the registry key once urlmon.dll
// is loaded doesn't do anything. So, both QuarantineTests and
// QuarantineTestWinTests need to set the zone scopes, in case they run in the
// same process.
void QuarantineTestBase::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_CURRENT_USER));

  scoped_zone_for_trusted_site_ = std::make_unique<ScopedZoneForSite>(
      GetTrustedSite(), "https",
      ScopedZoneForSite::ZoneIdentifierType::kTrustedSitesZone);
  scoped_zone_for_restricted_site_ = std::make_unique<ScopedZoneForSite>(
      GetRestrictedSite(), "https",
      ScopedZoneForSite::ZoneIdentifierType::kRestrictedSitesZone);
  scoped_zone_for_internet_site_ = std::make_unique<ScopedZoneForSite>(
      GetInternetSite(), "https",
      ScopedZoneForSite::ZoneIdentifierType::kInternetZone);
  scoped_zone_for_local_intranet_site_ = std::make_unique<ScopedZoneForSite>(
      GetLocalIntranetSite(), "http",
      ScopedZoneForSite::ZoneIdentifierType::kLocalIntranetZone);
}

namespace {

bool ZoneIdentifierPresentForFile(const base::FilePath& path,
                                  const GURL source_url,
                                  const GURL referrer_url) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  base::FilePath::StringType zone_identifier_path =
      path.value() + kZoneIdentifierStreamSuffix;
  base::win::ScopedHandle file(
      ::CreateFile(zone_identifier_path.c_str(), GENERIC_READ, kShare, nullptr,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.is_valid()) {
    return false;
  }

  // During testing, the zone identifier is expected to be under this limit.
  std::vector<char> zone_identifier_contents_buffer(4096);
  DWORD actual_length = 0;
  if (!::ReadFile(file.Get(), &zone_identifier_contents_buffer.front(),
                  zone_identifier_contents_buffer.size(), &actual_length,
                  nullptr))
    return false;
  zone_identifier_contents_buffer.resize(actual_length);

  std::string zone_identifier_contents(zone_identifier_contents_buffer.begin(),
                                       zone_identifier_contents_buffer.end());

  std::vector<std::string_view> lines =
      base::SplitStringPiece(zone_identifier_contents, "\n",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() < 2 || lines[0] != "[ZoneTransfer]" != 0)
    return false;

  std::string_view found_zone_id;
  std::string_view found_host_url;
  std::string_view found_referrer_url;

  // Note that we don't try too hard to parse the zone identifier here. This is
  // a test. If Windows starts adding whitespace or doing anything fancier than
  // ASCII, then we'd have to update this.
  for (const auto& line : lines) {
    if (base::StartsWith(line, "ZoneId="))
      found_zone_id = line.substr(7);
    else if (base::StartsWith(line, "HostUrl="))
      found_host_url = line.substr(8);
    else if (base::StartsWith(line, "ReferrerUrl="))
      found_referrer_url = line.substr(12);
  }

  return !found_zone_id.empty() &&
         (source_url.is_empty() ||
          SanitizeUrlForQuarantine(source_url).spec() == found_host_url) &&
         (referrer_url.is_empty() ||
          SanitizeUrlForQuarantine(referrer_url).spec() == found_referrer_url);
}

}  // namespace

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  return ZoneIdentifierPresentForFile(file, source_url, referrer_url);
}

}  // namespace quarantine
