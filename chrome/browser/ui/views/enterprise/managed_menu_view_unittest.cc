// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/managed_menu_view.h"

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagedMenuViewUnitTest : public TestWithBrowserView {
 public:
  ManagedMenuViewUnitTest() {}
};

TEST_F(ManagedMenuViewUnitTest, ManagedAccountLabel) {
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(
          browser_view()->GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  browser_view()->GetProfile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabel, "Manager");

  std::unique_ptr<ManagedMenuView> view =
      std::make_unique<ManagedMenuView>(nullptr, browser_view()->browser());
  view->Init();

  EXPECT_TRUE(view->browser_management_label().empty());
  EXPECT_TRUE(view->profile_management_label().empty());
  EXPECT_EQ(view->GetWindowTitle(), u"Your profile is managed by Manager");
}

TEST_F(ManagedMenuViewUnitTest, ManagedBrowserLabel) {
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(
          browser_view()->GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  g_browser_process->local_state()->SetString(prefs::kEnterpriseCustomLabel,
                                              "Manager");

  std::unique_ptr<ManagedMenuView> view =
      std::make_unique<ManagedMenuView>(nullptr, browser_view()->browser());
  view->Init();

  EXPECT_TRUE(view->browser_management_label().empty());
  EXPECT_TRUE(view->profile_management_label().empty());
  EXPECT_EQ(view->GetWindowTitle(), u"Your browser is managed by Manager");
  g_browser_process->local_state()->ClearPref(prefs::kEnterpriseCustomLabel);
}

TEST_F(ManagedMenuViewUnitTest, ManagedProfileBrowserDifferentLabel) {
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(
          browser_view()->GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN |
          policy::EnterpriseManagementAuthority::CLOUD);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  g_browser_process->local_state()->SetString(prefs::kEnterpriseCustomLabel,
                                              "Device Manager");
  browser_view()->GetProfile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabel, "Account Manager");

  std::unique_ptr<ManagedMenuView> view =
      std::make_unique<ManagedMenuView>(nullptr, browser_view()->browser());
  view->Init();

  EXPECT_EQ(view->browser_management_label(),
            u"Device Manager is managing your browser");
  EXPECT_EQ(view->profile_management_label(),
            u"Account Manager is managing your profile");
  EXPECT_EQ(view->GetWindowTitle(), u"Your browser and profile are managed");
  g_browser_process->local_state()->ClearPref(prefs::kEnterpriseCustomLabel);
}

TEST_F(ManagedMenuViewUnitTest, ManagedProfileBrowserSameLabel) {
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(
          browser_view()->GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN |
          policy::EnterpriseManagementAuthority::CLOUD);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  g_browser_process->local_state()->SetString(prefs::kEnterpriseCustomLabel,
                                              "Manager");
  browser_view()->GetProfile()->GetPrefs()->SetString(
      prefs::kEnterpriseCustomLabel, "Manager");

  std::unique_ptr<ManagedMenuView> view =
      std::make_unique<ManagedMenuView>(nullptr, browser_view()->browser());
  view->Init();

  EXPECT_TRUE(view->browser_management_label().empty());
  EXPECT_TRUE(view->profile_management_label().empty());
  EXPECT_EQ(view->GetWindowTitle(),
            u"Your browser and profile are managed by Manager");
  g_browser_process->local_state()->ClearPref(prefs::kEnterpriseCustomLabel);
}
