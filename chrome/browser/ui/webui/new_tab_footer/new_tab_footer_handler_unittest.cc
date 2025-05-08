// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "content/public/test/browser_task_environment.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

using testing::_;

class MockNewTabFooterDocument
    : public new_tab_footer::mojom::NewTabFooterDocument {
 public:
  MockNewTabFooterDocument() = default;
  ~MockNewTabFooterDocument() override = default;

  mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetManagementNotice,
              (new_tab_footer::mojom::ManagementNoticePtr));
  mojo::Receiver<new_tab_footer::mojom::NewTabFooterDocument> receiver_{this};
};

class NewTabFooterHandlerExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>(),
        web_contents_.get());
  }

  NewTabFooterHandler& handler() { return *handler_; }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabFooterHandler> handler_;
};

TEST_F(NewTabFooterHandlerExtensionTest,
       GetExtensionAttributionWithoutExtension) {
  new_tab_footer::mojom::ExtensionAttributionPtr extension_attribution;

  base::MockCallback<NewTabFooterHandler::GetNtpExtensionAttributionCallback>
      callback;
  EXPECT_CALL(callback, Run).WillOnce(MoveArg<0>(&extension_attribution));
  handler().GetNtpExtensionAttribution(callback.Get());
  EXPECT_FALSE(extension_attribution);
}

TEST_F(NewTabFooterHandlerExtensionTest, GetExtensionAttributionWithExtension) {
  // Load NTP extension.
  extensions::TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext.html"
        },
        "name": "Extension-overridden NTP",
        "manifest_version": 3,
        "version": "0.1"
      })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                          "<body>Extension-overridden NTP</body>");
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(extension_dir.Pack());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override because the usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  new_tab_footer::mojom::ExtensionAttributionPtr extension_attribution;
  base::MockCallback<NewTabFooterHandler::GetNtpExtensionAttributionCallback>
      callback;
  EXPECT_CALL(callback, Run).WillOnce(MoveArg<0>(&extension_attribution));
  handler().GetNtpExtensionAttribution(callback.Get());
  ASSERT_TRUE(extension_attribution);
  EXPECT_EQ(extension_attribution->name, extension->name());
  EXPECT_EQ(extension_attribution->url,
            net::AppendOrReplaceQueryParameter(
                GURL(chrome::kChromeUIExtensionsURL), "id", extension->id()));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class NewTabFooterHandlerEnterpriseTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Test Profile");
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    feature_list_.InitAndEnableFeature(
        features::kEnterpriseBadgingForNtpFooter);
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        document_.BindAndGetRemote(), web_contents_.get());
  }

  void TearDown() override {
    // Ensure that the handler is destroyed before the profile.
    handler_.reset();
    web_contents_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  NewTabFooterHandler& handler() { return *handler_; }
  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabFooterHandler> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNewTabFooterDocument> document_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNoticeWithDefaultText) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Browser management is local, so no domain is indicated in the managent
  // notice text.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNoticeWithCustomtext) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Set a custom label policy, which will be used in the management notice
  // text.
  profile_manager_->local_state()->Get()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "custom label");
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by custom label", notice->text);
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNoticeWithDomainText) {
  // Simulate browser management.
  const std::string managing_domain = "example.com";

  // Simulate that the browser is managed by a cloud domain. The domain will be
  // indicated in the management notice text.
  ScopedDeviceManagerForTesting device_manager_for_testing(
      managing_domain.c_str());
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by example.com", notice->text);
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
