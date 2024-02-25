// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_query_params.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "components/update_client/update_query_params_delegate.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPrintf;

namespace update_client {

namespace {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

class TestUpdateQueryParamsDelegate : public UpdateQueryParamsDelegate {
  std::string GetExtraParams() override { return "&cat=dog"; }
};

}  // namespace

void TestParams(UpdateQueryParams::ProdId prod_id, bool extra_params) {
  std::string params = UpdateQueryParams::Get(prod_id);

  // This doesn't so much test what the values are (since that would be an
  // almost exact duplication of code with update_query_params.cc, and wouldn't
  // really test anything) as it is a verification that all the params are
  // present in the generated string.
  EXPECT_TRUE(
      Contains(params, StringPrintf("os=%s", UpdateQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(params, StringPrintf("arch=%s", UpdateQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("os_arch=%s",
                   base::SysInfo().OperatingSystemArchitecture().c_str())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("prod=%s", UpdateQueryParams::GetProdIdString(prod_id))));
  if (extra_params) {
    EXPECT_TRUE(Contains(params, "cat=dog"));
  }
}

void TestProdVersion() {
  EXPECT_EQ(version_info::GetVersionNumber(),
            UpdateQueryParams::GetProdVersion());
}

TEST(UpdateQueryParamsTest, GetParams) {
  TestProdVersion();

  TestParams(UpdateQueryParams::CRX, false);
  TestParams(UpdateQueryParams::CHROME, false);

  TestUpdateQueryParamsDelegate delegate;
  UpdateQueryParams::SetDelegate(&delegate);

  TestParams(UpdateQueryParams::CRX, true);
  TestParams(UpdateQueryParams::CHROME, true);
}

}  // namespace update_client
