// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"
#include "chrome/browser/ui/webui/new_tab_footer/mock_new_tab_footer_document.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

using testing::_;

namespace {
const char kExtensionNtpName[] = "Extension-overridden NTP";
}

class TestEmbedder final : public TopChromeWebUIController::Embedder {
 public:
  TestEmbedder() = default;
  ~TestEmbedder() = default;

  void ShowUI() override {}
  void CloseUI() override {}
  void HideContextMenu() override {}

  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override {
    context_menu_shown_ = true;
  }

  bool context_menu_shown() const { return context_menu_shown_; }

  base::WeakPtr<TestEmbedder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool context_menu_shown_;

  base::WeakPtrFactory<TestEmbedder> weak_factory_{this};
};

class NewTabFooterHandlerExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    embedder_ = std::make_unique<TestEmbedder>();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        document_.BindAndGetRemote(), embedder_->GetWeakPtr(),
        web_contents_.get());
    testing::Mock::VerifyAndClearExpectations(&document_);
  }

  scoped_refptr<const extensions::Extension> LoadNtpExtension() {
    extensions::TestExtensionDir extension_dir;
    const std::string kManifest = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext.html"
        },
        "name": ")" + std::string(kExtensionNtpName) +
                                  R"(",
          "manifest_version": 3,
          "version": "0.1"
      })";
    extension_dir.WriteManifest(kManifest);
    extension_dir.WriteFile(
        FILE_PATH_LITERAL("ext.html"),
        std::string("<body>") + kExtensionNtpName + "</body>");
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    scoped_refptr<const extensions::Extension> extension =
        extension_loader.LoadExtension(extension_dir.Pack());
    return extension;
  }

  void UnloadExtension(std::string extension_id) {
    extensions::ExtensionRegistrar::Get(profile())->RemoveExtension(
        extension_id, extensions::UnloadedExtensionReason::DISABLE);
  }

  TestEmbedder& embedder() { return *embedder_; }
  NewTabFooterHandler& handler() { return *handler_; }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestEmbedder> embedder_;
  std::unique_ptr<NewTabFooterHandler> handler_;
  testing::NiceMock<MockNewTabFooterDocument> document_;
};

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_NoExtension) {
  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });
  handler().UpdateNtpExtensionName();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ManualUpdate) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, kExtensionNtpName); });
  handler().UpdateNtpExtensionName();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ReadyExtension) {
  std::string name = "will change";
  EXPECT_CALL(document_, SetNtpExtensionName)
      .Times(2)
      .WillRepeatedly(testing::Invoke(
          [&name](const std::string& name_arg) { name = name_arg; }));

  // Load the NTP extension and verify it's enabled.
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  document_.FlushForTesting();

  // Although OnExtensionReady was triggered, the footer isn't aware of the
  // extension until the URL override is activated.
  EXPECT_EQ(name, std::string());

  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  registry()->TriggerOnReady(extension.get());
  document_.FlushForTesting();

  EXPECT_EQ(name, kExtensionNtpName);
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_UnloadExtension) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension_id));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });

  extensions::TestExtensionRegistryObserver observer(registry(), extension_id);
  UnloadExtension(extension_id);
  observer.WaitForExtensionUnloaded();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_DisableByPolicy) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });

  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ReenablePolicy) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, kExtensionNtpName); });

  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, true);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerExtensionTest, ContextMenu_Shows) {
  handler().ShowContextMenu(gfx::Point());
  EXPECT_TRUE(embedder().context_menu_shown());
}

TEST_F(NewTabFooterHandlerExtensionTest, ContextMenu_HidesFooter) {
  base::HistogramTester histogram_tester;
  const std::string& hide_footer = "NewTabPage.Footer.ContextMenuClicked";
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  histogram_tester.ExpectTotalCount(hide_footer, 0);

  FooterContextMenu menu(profile());
  menu.ExecuteCommand(0 /* COMMAND_CLOSE_FOOTER */, /*event_flags=*/0);

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  histogram_tester.ExpectTotalCount(hide_footer, 1);
  histogram_tester.ExpectBucketCount(
      hide_footer, new_tab_footer::FooterContextMenuItem::kHideFooter, 1);
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
        document_.BindAndGetRemote(),
        base::WeakPtr<TopChromeWebUIController::Embedder>(),
        web_contents_.get());
    testing::Mock::VerifyAndClearExpectations(&document_);
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

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNotice_None) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::NONE);

  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_FALSE(notice);
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNotice_DefaultText) {
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
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNotice_CustomText) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Set a custom label policy, which will be used in the management notice
  // text.
  profile_manager_->local_state()->Get()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "custom label");
  document_.FlushForTesting();

  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by custom label", notice->text);
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNotice_DomainText) {
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
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetNtpManagementNotice_CustomLogo) {
  // Simulate that the browser is managed.
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  gfx::Image custom_logo = gfx::test::CreateImage(256, 256);
  policy::ManagementServiceFactory::GetForProfile(profile())
      ->SetBrowserManagementIconForTesting(custom_logo);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      custom_logo.AsBitmap(), handler_->GetManagementNoticeIconBitmap()));
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
        // We only test the base URL as the data is very long and not readable.
        EXPECT_TRUE(base::StartsWith(notice->bitmap_data_url.spec(),
                                     "data:image/png;base64,"));
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetNtpManagementNotice_DefaultLogo) {
  // Simulate that the browser is managed.
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  const gfx::ImageSkia default_logo =
      gfx::CreateVectorIcon(gfx::IconDescription(
          vector_icons::kBusinessIcon, 20,
          web_contents_->GetColorProvider().GetColor(ui::kColorIcon)));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      default_logo.GetRepresentation(1.0f).GetBitmap(),
      handler_->GetManagementNoticeIconBitmap()));
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
        // We only test the base URL as the data is very long and not readable.
        EXPECT_TRUE(base::StartsWith(notice->bitmap_data_url.spec(),
                                     "data:image/png;base64,"));
      });
  handler().UpdateManagementNotice();

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetManagementNotice_SetLabelPolicy) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Set a custom label policy and verify that the document receives the updated
  // text that uses the new label.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by NewCustomLabel", notice->text);
      });
  profile_manager_->local_state()->Get()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "NewCustomLabel");

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetManagementNotice_UunsetLabelPolicy) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  profile_manager_->local_state()->Get()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "CustomLabel");
  // Browser management is local, so no domain is indicated in the managent
  // notice text.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by CustomLabel", notice->text);
      });

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);

  // Unset the custom label policy and verify that the document receives the
  // updated text, which is the default.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
      });
  profile_manager_->local_state()->Get()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, std::string());

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetManagementNotice_DisableNoticePolicy) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Disable the management notice policy and verify that an empty notice is
  // set.
  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);

  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_FALSE(notice);
      });

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetManagementNotice_ReenableNoticePolicy) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);

  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_FALSE(notice);
      });

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);

  // Unset the management notice policy and verify that the notice is set.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
      });

  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, true);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
