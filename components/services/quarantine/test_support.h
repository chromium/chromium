// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_TEST_SUPPORT_H_
#define COMPONENTS_SERVICES_QUARANTINE_TEST_SUPPORT_H_

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <memory>
#include <string_view>

#include "base/test/test_reg_util_win.h"

namespace quarantine {
class ScopedZoneForSite;

class QuarantineTestBase : public testing::Test {
 public:
  QuarantineTestBase();
  ~QuarantineTestBase() override;

  void SetUp() override;

  static std::string_view GetTrustedSite();
  static std::string_view GetRestrictedSite();
  static std::string_view GetInternetSite();
  static std::string_view GetLocalIntranetSite();

 protected:
  registry_util::RegistryOverrideManager registry_override_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_trusted_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_restricted_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_internet_site_;
  std::unique_ptr<ScopedZoneForSite> scoped_zone_for_local_intranet_site_;
};
}  // namespace quarantine
#else
namespace quarantine {
using QuarantineTestBase = testing::Test;
}  // namespace quarantine
#endif

class GURL;

namespace base {
class FilePath;
}

namespace quarantine {

// Determine if a file has quarantine metadata attached to it.
//
// If |source_url| is non-empty, then the download source URL in
// quarantine metadata should match |source_url| exactly. The function returns
// |false| if there is a mismatch. If |source_url| is empty, then this function
// only checks for the existence of a download source URL in quarantine
// metadata.
//
// If |referrer_url| is valid, then the download referrer URL in quarantine
// metadata must match |referrer_url| exactly. The function returns |false| if
// there is a mismatch in the |referrer_url| even if the |source_url| matches.
// No referrer URL checks are performed if |referrer_url| is empty.
//
// If both |source_url| and |referrer_url| are empty, then the function returns
// true if any quarantine metadata is present for the file.
//
// **Note**: On Windows 8 or lower, this function only checks if the
// |ZoneIdentifier| metadata is present. |source_url| and |referrer_url| are
// ignored. Individual URLs are only stored as part of the mark-of-the-web since
// Windows 10.
bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url);

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_TEST_SUPPORT_H_
