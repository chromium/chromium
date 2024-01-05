// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_details.h"

#include <string>

#include "base/version_info/version_info_values.h"
#include "chrome/install_static/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace install_static {

class FakeInstallDetails : public InstallDetails {
 public:
  FakeInstallDetails() : InstallDetails(&payload) {
    constants.size = sizeof(constants);
    constants.install_suffix = L"";
    constants.default_channel_name = L"";
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    constants.app_guid = L"testguid";
    constants.channel_strategy = ChannelStrategy::FIXED;
#else
    constants.app_guid = L"";
    constants.channel_strategy = ChannelStrategy::UNSUPPORTED;
#endif
    payload.size = sizeof(payload);
    payload.product_version = product_version.c_str();
    payload.mode = &constants;
    payload.channel = channel.c_str();
    payload.channel_length = channel.length();
  }

  FakeInstallDetails(const FakeInstallDetails&) = delete;
  FakeInstallDetails& operator=(const FakeInstallDetails&) = delete;

  void set_product_version(const char* version) {
    product_version.assign(version);
    payload.product_version = product_version.c_str();
  }

  void set_payload_size(size_t size) { payload.size = size; }

  void set_mode_size(size_t size) { constants.size = size; }

  InstallConstants constants = InstallConstants();
  std::wstring channel = std::wstring(L"testchannel");
  std::string product_version = std::string(PRODUCT_VERSION);
  Payload payload = Payload();
};

TEST(InstallDetailsTest, GetClientStateKeyPath) {
  FakeInstallDetails details;
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  EXPECT_THAT(details.GetClientStateKeyPath(),
              StrEq(L"Software\\Google\\Update\\ClientState\\testguid"));
#else
  EXPECT_THAT(details.GetClientStateKeyPath(),
              StrEq(std::wstring(L"Software\\").append(kProductPathName)));
#endif
}

TEST(InstallDetailsTest, GetClientStateMediumKeyPath) {
  FakeInstallDetails details;
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  EXPECT_THAT(details.GetClientStateMediumKeyPath(),
              StrEq(L"Software\\Google\\Update\\ClientStateMedium\\testguid"));
#else
  EXPECT_THAT(details.GetClientStateKeyPath(),
              StrEq(std::wstring(L"Software\\").append(kProductPathName)));
#endif
}

TEST(InstallDetailsTest, VersionMismatch) {
  // All is well to begin with.
  EXPECT_FALSE(FakeInstallDetails().VersionMismatch());

  // Bad product version.
  {
    FakeInstallDetails details;
    details.set_product_version("0.1.2.3");
    EXPECT_TRUE(details.VersionMismatch());
  }

  // Bad Payload size.
  {
    FakeInstallDetails details;
    details.set_payload_size(sizeof(InstallDetails::Payload) + 1);
    EXPECT_TRUE(details.VersionMismatch());
  }

  // Bad InstallConstants size.
  {
    FakeInstallDetails details;
    details.set_mode_size(sizeof(InstallConstants) + 1);
    EXPECT_TRUE(details.VersionMismatch());
  }
}

}  // namespace install_static
