// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/arc_net_host_impl.h"

#include <string>

#include "base/macros.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcNetHostImplTest : public testing::Test {
 protected:
  ArcNetHostImplTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        context_(std::make_unique<TestBrowserContext>()),
        service_(
            ArcNetHostImpl::GetForBrowserContextForTesting(context_.get())) {
    arc::prefs::RegisterProfilePrefs(pref_service()->registry());
    service()->SetPrefService(pref_service());
  }

  ~ArcNetHostImplTest() override { service_->Shutdown(); }

  ArcNetHostImpl* service() { return service_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestBrowserContext> context_;
  ArcNetHostImpl* const service_;

  DISALLOW_COPY_AND_ASSIGN(ArcNetHostImplTest);
};

TEST_F(ArcNetHostImplTest, SetAlwaysOnVpn_SetPackage) {
  EXPECT_EQ(false, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ("", pref_service()->GetString(prefs::kAlwaysOnVpnPackage));

  const std::string vpn_package = "com.android.vpn";
  const bool lockdown = true;

  service()->SetAlwaysOnVpn(vpn_package, lockdown);

  EXPECT_EQ(lockdown, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ(vpn_package, pref_service()->GetString(prefs::kAlwaysOnVpnPackage));
}

}  // namespace
}  // namespace arc
