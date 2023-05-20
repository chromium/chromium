// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/update_client/update_query_params.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

}  // namespace

void TestParams(update_client::UpdateQueryParams::ProdId prod_id) {
  std::string params = update_client::UpdateQueryParams::Get(prod_id);

  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"os=", update_client::UpdateQueryParams::GetOS()})));
  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"arch=", update_client::UpdateQueryParams::GetArch()})));
  EXPECT_TRUE(Contains(
      params, base::StrCat({"os_arch=",
                            base::SysInfo().OperatingSystemArchitecture()})));
  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"prod=", update_client::UpdateQueryParams::GetProdIdString(
                                 prod_id)})));
  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"prodchannel=", chrome::GetChannelName(
                                        chrome::WithExtendedStable(true))})));
  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"prodversion=", version_info::GetVersionNumber()})));
  EXPECT_TRUE(Contains(
      params,
      base::StrCat({"lang=", ChromeUpdateQueryParamsDelegate::GetLang()})));
}

TEST(ChromeUpdateQueryParamsDelegateTest, GetParams) {
  TestParams(update_client::UpdateQueryParams::CRX);
  TestParams(update_client::UpdateQueryParams::CHROME);
}
