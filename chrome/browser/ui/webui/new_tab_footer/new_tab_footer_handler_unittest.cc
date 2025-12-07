// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_footer/mock_new_tab_footer_document.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/theme_provider.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
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
using ::testing::SaveArg;

namespace {
const char kExtensionNtpName[] = "Extension-overridden NTP";
}

class MockThemeProvider : public ui::ThemeProvider {
 public:
  MOCK_CONST_METHOD1(GetImageSkiaNamed, gfx::ImageSkia*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD1(GetTint, color_utils::HSL(int));
  MOCK_CONST_METHOD1(GetDisplayProperty, int(int));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD2(GetRawData,
                     base::RefCountedMemory*(int, ui::ResourceScaleFactor));
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(std::optional<CustomBackground>, GetCustomBackground, ());
  MOCK_METHOD(void, AddObserver, (NtpCustomBackgroundServiceObserver*));
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
        document_.BindAndGetRemote(),
        base::WeakPtr<TopChromeWebUIController::Embedder>(),
        NtpCustomBackgroundServiceFactory::GetForProfile(profile()),
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

  NewTabFooterHandler& handler() { return *handler_; }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
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
      .WillRepeatedly(
          [&name](const std::string& name_arg) { name = name_arg; });

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

TEST_F(NewTabFooterHandlerExtensionTest, AttachedTabStateUpdated) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));
  registry()->TriggerOnReady(extension.get());
  document_.FlushForTesting();

  new_tab_footer::mojom::NewTabPageType ntp_type;
  EXPECT_CALL(document_, AttachedTabStateUpdated)
      .Times(3)
      .WillRepeatedly(SaveArg<0>(&ntp_type));

  handler().AttachedTabStateUpdated(GURL(extension->url()));
  document_.FlushForTesting();
  EXPECT_EQ(ntp_type, new_tab_footer::mojom::NewTabPageType::kExtension);

  handler().AttachedTabStateUpdated(GURL(chrome::kChromeUINewTabPageURL));
  document_.FlushForTesting();
  EXPECT_EQ(ntp_type, new_tab_footer::mojom::NewTabPageType::kFirstPartyWebUI);

  handler().AttachedTabStateUpdated(
      GURL(chrome::kChromeUINewTabPageThirdPartyHost));
  document_.FlushForTesting();
  EXPECT_EQ(ntp_type, new_tab_footer::mojom::NewTabPageType::kOther);

  testing::Mock::VerifyAndClearExpectations(&document_);
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class NewTabFooterHandlerEnterpriseTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kEnterpriseBadgingForNtpFooter);
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Test Profile");
    InitializeHandler();
    testing::Mock::VerifyAndClearExpectations(&document_);
  }

  void InitializeHandler() {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    mock_ntp_custom_background_service_ =
        std::make_unique<testing::NiceMock<MockNtpCustomBackgroundService>>(
            profile_);
    EXPECT_CALL(*mock_ntp_custom_background_service_.get(), AddObserver)
        .Times(1)
        .WillOnce(
            testing::SaveArg<0>(&ntp_custom_background_service_observer_));
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        document_.BindAndGetRemote(),
        base::WeakPtr<TopChromeWebUIController::Embedder>(),
        mock_ntp_custom_background_service_.get(), web_contents_.get());
    ASSERT_EQ(handler_.get(), ntp_custom_background_service_observer_);
    handler_->SetThemeProviderForTesting(&mock_theme_provider_);

    document_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&document_);
  }

  void TearDown() override {
    ntp_custom_background_service_observer_ = nullptr;
    // Ensure that the handler is destroyed before the profile.
    handler_->SetThemeProviderForTesting(nullptr);
    handler_.reset();
    web_contents_.reset();
    mock_ntp_custom_background_service_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  NewTabFooterHandler& handler() { return *handler_; }
  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<testing::NiceMock<MockNtpCustomBackgroundService>>
      mock_ntp_custom_background_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNewTabFooterDocument> document_;
  testing::NiceMock<MockThemeProvider> mock_theme_provider_;
  raw_ptr<NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observer_;
  std::unique_ptr<NewTabFooterHandler> handler_;
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
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
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
        EXPECT_TRUE(base::StartsWith(notice->custom_bitmap_data_url->spec(),
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

  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce([](new_tab_footer::mojom::ManagementNoticePtr notice) {
        EXPECT_EQ("Managed by your organization", notice->text);
        // We only test the base URL as the data is very long and not readable.
        EXPECT_FALSE(notice->custom_bitmap_data_url);
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
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
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

  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
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
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
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
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
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
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
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

  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, true);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SettingLogoPolicyUpdatesIcon) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Trigger a logo change to set the custom logo.
  new_tab_footer::mojom::ManagementNoticePtr updated_notice;
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce(
          [&updated_notice](new_tab_footer::mojom::ManagementNoticePtr notice) {
            updated_notice = std::move(notice);
          });
  gfx::Image custom_logo = gfx::test::CreateImage(256, 256);
  policy::ManagementServiceFactory::GetForProfile(profile())
      ->SetBrowserManagementIconForTesting(custom_logo);

  handler_->OnEnterpriseLogoUpdatedForBrowser();

  // Verify that the custom icon is set.
  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
  EXPECT_EQ("Managed by your organization", updated_notice->text);
  // We only test the base URL as the data is very long and not readable.
  EXPECT_TRUE(base::StartsWith(updated_notice->custom_bitmap_data_url->spec(),
                               "data:image/png;base64,"));

  // Trigger a logo change to unset the custom logo.
  EXPECT_CALL(document_, SetManagementNotice)
      .WillOnce(
          [&updated_notice](new_tab_footer::mojom::ManagementNoticePtr notice) {
            updated_notice = std::move(notice);
          });
  policy::ManagementServiceFactory::GetForProfile(profile())
      ->SetBrowserManagementIconForTesting(gfx::Image());

  handler_->OnEnterpriseLogoUpdatedForBrowser();

  // Verify that there is no longer a custom icon sent.
  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
  EXPECT_EQ("Managed by your organization", updated_notice->text);
  // We only test the base URL as the data is very long and not readable.
  EXPECT_FALSE(updated_notice->custom_bitmap_data_url);
}

TEST_F(NewTabFooterHandlerEnterpriseTest, SetCustomBackground_None) {
  new_tab_footer::mojom::BackgroundAttributionPtr attribution;
  EXPECT_CALL(document_, SetBackgroundAttribution)
      .Times(1)
      .WillOnce(
          [&attribution](new_tab_footer::mojom::BackgroundAttributionPtr arg) {
            attribution = std::move(arg);
          });
  ON_CALL(*mock_ntp_custom_background_service_.get(), GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional<CustomBackground>()));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  document_.FlushForTesting();

  EXPECT_FALSE(attribution);
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetCustomBackground_ThemeWithCustomImagePresent) {
  EXPECT_CALL(document_, SetBackgroundAttribution)
      .Times(1)
      .WillOnce(
          [](new_tab_footer::mojom::BackgroundAttributionPtr attribution) {
            EXPECT_FALSE(attribution);
          });
  CustomBackground custom_background;
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  ON_CALL(*mock_ntp_custom_background_service_.get(), GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetCustomBackground_TwoLinesAttribution) {
  EXPECT_CALL(document_, SetBackgroundAttribution)
      .Times(1)
      .WillOnce(
          [](new_tab_footer::mojom::BackgroundAttributionPtr attribution) {
            ASSERT_TRUE(attribution);
            EXPECT_EQ("foo line, bar line", attribution->name);
            EXPECT_EQ(attribution->url->spec(), "https://foo.com/action");
          });

  CustomBackground custom_background;
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_line_2 = "bar line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  ON_CALL(*mock_ntp_custom_background_service_.get(), GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerEnterpriseTest,
       SetCustomBackground_OneLineAtribution) {
  EXPECT_CALL(document_, SetBackgroundAttribution)
      .Times(1)
      .WillOnce(
          [](new_tab_footer::mojom::BackgroundAttributionPtr attribution) {
            ASSERT_TRUE(attribution);
            EXPECT_EQ("foo line", attribution->name);
            EXPECT_EQ(attribution->url->spec(), "https://foo.com/action");
          });

  CustomBackground custom_background;
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  ON_CALL(*mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  document_.FlushForTesting();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
