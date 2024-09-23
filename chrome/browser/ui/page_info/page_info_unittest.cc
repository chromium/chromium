// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/page_info/core/features.h"
#include "components/page_info/page_info_ui.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::SSLStatus;
using content_settings::SettingSource;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace {

// SSL cipher suite like specified in RFC5246 Appendix A.5. "The Cipher Suite".
// Without the CR_ prefix, this clashes with the OS X 10.8 headers.
int CR_TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x3D;

int SetSSLVersion(int connection_status, int version) {
  // Clear SSL version bits (Bits 20, 21 and 22).
  connection_status &=
      ~(net::SSL_CONNECTION_VERSION_MASK << net::SSL_CONNECTION_VERSION_SHIFT);
  int bitmask = version << net::SSL_CONNECTION_VERSION_SHIFT;
  return bitmask | connection_status;
}

int SetSSLCipherSuite(int connection_status, int cipher_suite) {
  // Clear cipher suite bits (the 16 lowest bits).
  connection_status &= ~net::SSL_CONNECTION_CIPHERSUITE_MASK;
  return cipher_suite | connection_status;
}

class MockPageInfoUI : public PageInfoUI {
 public:
  ~MockPageInfoUI() override = default;
  MOCK_METHOD(void, SetCookieInfo, (const CookiesNewInfo& cookie_info));
  MOCK_METHOD(void, SetPermissionInfoStub, ());
  MOCK_METHOD(void, SetIdentityInfo, (const IdentityInfo& identity_info));
  MOCK_METHOD(void, SetPageFeatureInfo, (const PageFeatureInfo& info));
  MOCK_METHOD(void,
              SetAdPersonalizationInfo,
              (const AdPersonalizationInfo& info));

  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list,
      ChosenObjectInfoList chosen_object_info_list) override {
    SetPermissionInfoStub();
    if (set_permission_info_callback_) {
      set_permission_info_callback_.Run(permission_info_list,
                                        std::move(chosen_object_info_list));
    }
  }

  base::RepeatingCallback<void(const PermissionInfoList& permission_info_list,
                               ChosenObjectInfoList chosen_object_info_list)>
      set_permission_info_callback_;
};

class PageInfoTest : public ChromeRenderViewHostTestHarness {
 public:
  PageInfoTest() : testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    SetURL("http://www.example.com");
  }

  ~PageInfoTest() override = default;

  void SetUp() override {
    // TODO(crbug.com/40231917): Fix tests and enable the feature.
    scoped_feature_list_.InitWithFeatures(
        {
#if !BUILDFLAG(IS_ANDROID)
            features::kFileSystemAccessPersistentPermissions,
#endif
        },
        {});

    ChromeRenderViewHostTestHarness::SetUp();

    // Setup stub security info.
    security_level_ = security_state::NONE;

    // Create the certificate.
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmail(profile()->GetProfileUserName()),
        /*is_affiliated=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    CreateWebContentsUserData(web_contents());

    // Setup mock ui.
    ResetMockUI();
  }

  void TearDown() override {
    ASSERT_TRUE(page_info_ || incognito_page_info_)
        << "No PageInfo instance created.";
    incognito_web_contents_.reset();
    page_info_.reset();
    incognito_page_info_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        StatefulSSLHostStateDelegateFactory::GetInstance(),
        StatefulSSLHostStateDelegateFactory::GetDefaultFactoryForTesting()}};
  }

  void SetDefaultUIExpectations(MockPageInfoUI* mock_ui) {
    // During creation |PageInfo| makes the following calls to the ui.
    EXPECT_CALL(*mock_ui, SetPermissionInfoStub());
    EXPECT_CALL(*mock_ui, SetIdentityInfo(_));
    ExpectInitialSetCookieInfoCall(mock_ui);
  }

  void ExpectInitialSetCookieInfoCall(MockPageInfoUI* mock_ui) {
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*mock_ui, SetCookieInfo(_)).Times(1);
#else
    EXPECT_CALL(*mock_ui, SetCookieInfo(_));
#endif
  }

  void SetURL(const std::string& url) {
    url_ = GURL(url);
    origin_ = url::Origin::Create(url_);
  }

  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) {
    last_chosen_object_info_.clear();
    for (auto& chosen_object_info : chosen_object_info_list) {
      last_chosen_object_info_.push_back(std::move(chosen_object_info));
    }
    last_permission_info_list_ = permission_info_list;
  }

  void ResetMockUI() {
    mock_ui_ = std::make_unique<NiceMock<MockPageInfoUI>>();
    // Use this rather than gmock's ON_CALL.WillByDefault(Invoke(... because
    // gmock doesn't handle move-only types well.
    mock_ui_->set_permission_info_callback_ = base::BindRepeating(
        &PageInfoTest::SetPermissionInfo, base::Unretained(this));
  }

  void ClearPageInfo() { page_info_.reset(nullptr); }

  void SetCertToSHA1() {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_leaf.pem");
    ASSERT_TRUE(cert_);
  }

  const GURL& url() const { return url_; }
  const url::Origin& origin() const { return origin_; }
  scoped_refptr<net::X509Certificate> cert() { return cert_; }
  MockPageInfoUI* mock_ui() { return mock_ui_.get(); }
  const std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>&
  last_chosen_object_info() {
    return last_chosen_object_info_;
  }
  const PermissionInfoList& last_permission_info_list() {
    return last_permission_info_list_;
  }
  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(web_contents());
  }

  PageInfo* page_info() {
    if (!page_info_.get()) {
      auto delegate = std::make_unique<ChromePageInfoDelegate>(web_contents());
      delegate->SetSecurityStateForTests(security_level_,
                                         visible_security_state_);
      page_info_ = std::make_unique<PageInfo>(std::move(delegate),
                                              web_contents(), url());
      base::RunLoop run_loop;
      page_info_->InitializeUiState(mock_ui(), run_loop.QuitClosure());
      run_loop.Run();
    }
    return page_info_.get();
  }

  PageInfo* incognito_page_info() {
    if (!incognito_page_info_.get()) {
      // Build the incognito profile manually in order to override testing
      // factories.
      TestingProfile::Builder incognito_profile_builder;
      incognito_profile_builder.AddTestingFactories(GetTestingFactories());
      incognito_profile_builder.BuildIncognito(profile());

      incognito_web_contents_ =
          content::WebContentsTester::CreateTestWebContents(
              profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
              nullptr);
      CreateWebContentsUserData(incognito_web_contents_.get());

      incognito_mock_ui_ = std::make_unique<NiceMock<MockPageInfoUI>>();
      incognito_mock_ui_->set_permission_info_callback_ = base::BindRepeating(
          &PageInfoTest::SetPermissionInfo, base::Unretained(this));

      auto delegate = std::make_unique<ChromePageInfoDelegate>(
          incognito_web_contents_.get());
      delegate->SetSecurityStateForTests(security_level_,
                                         visible_security_state_);
      incognito_page_info_ = std::make_unique<PageInfo>(
          std::move(delegate), incognito_web_contents_.get(), url());
      base::RunLoop run_loop;
      incognito_page_info_->InitializeUiState(incognito_mock_ui_.get(),
                                              run_loop.QuitClosure());
      run_loop.Run();
    }
    return incognito_page_info_.get();
  }

  void CreateWebContentsUserData(content::WebContents* contents) {
    // The test WebContents don't have all the helpers attached, so add in the
    // missing ones needed by these tests.
    infobars::ContentInfoBarManager::CreateForWebContents(contents);
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        contents,
        std::make_unique<PageSpecificContentSettingsDelegate>(contents));
    permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
        contents);
  }

  security_state::SecurityLevel security_level_;
  security_state::VisibleSecurityState visible_security_state_;

 private:
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<PageInfo> page_info_;
  std::unique_ptr<MockPageInfoUI> mock_ui_;

  std::unique_ptr<content::WebContents> incognito_web_contents_;
  std::unique_ptr<PageInfo> incognito_page_info_;
  std::unique_ptr<NiceMock<MockPageInfoUI>> incognito_mock_ui_;

#if !BUILDFLAG(IS_ANDROID)
  ChromeLayoutProvider layout_provider_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
#endif

  scoped_refptr<net::X509Certificate> cert_;
  GURL url_;
  url::Origin origin_;
  std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>
      last_chosen_object_info_;
  PermissionInfoList last_permission_info_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ExpectPermissionInfoList(
    const std::set<ContentSettingsType>& expected_types,
    const PermissionInfoList& permissions,
    const base::Location& location = FROM_HERE) {
  std::set<ContentSettingsType> actual_types;
  base::ranges::transform(permissions,
                          std::inserter(actual_types, actual_types.end()),
                          [](const auto& p) { return p.type; });

  EXPECT_THAT(actual_types, expected_types)
      << "(expected at " << location.ToString() << ")";
}

}  // namespace

TEST_F(PageInfoTest, PermissionStringsHaveMidSentenceVersion) {
  page_info();
  for (const PageInfoUI::PermissionUIInfo& info :
       PageInfoUI::GetContentSettingsUIInfoForTesting()) {
    std::u16string normal = l10n_util::GetStringUTF16(info.string_id);
    std::u16string mid_sentence =
        l10n_util::GetStringUTF16(info.string_id_mid_sentence);
    switch (info.type) {
      case ContentSettingsType::MIDI_SYSEX:
      case ContentSettingsType::NFC:
      case ContentSettingsType::USB_GUARD:
#if !BUILDFLAG(IS_ANDROID)
      case ContentSettingsType::HID_GUARD:
#endif
        EXPECT_EQ(normal, mid_sentence);
        break;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
        EXPECT_NE(normal, mid_sentence);
        EXPECT_EQ(base::ToLowerASCII(normal), base::ToLowerASCII(mid_sentence));
        break;
#endif
      default:
        EXPECT_NE(normal, mid_sentence);
        EXPECT_EQ(base::ToLowerASCII(normal), mid_sentence);
        break;
    }
  }
}

TEST_F(PageInfoTest, NonFactoryDefaultAndRecentlyChangedPermissionsShown) {
  GURL kEmbedded1("https://embedded1.com");
  GURL kEmbedded2("https://embedded2.com");

  page_info()->PresentSitePermissionsForTesting();
  std::set<ContentSettingsType> expected_visible_permissions;

#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Change some default-ask settings away from the default.
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
  page_info()->OnSitePermissionChanged(ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  expected_visible_permissions.insert(ContentSettingsType::NOTIFICATIONS);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  expected_visible_permissions.insert(ContentSettingsType::MEDIASTREAM_MIC);
  page_info()->OnSitePermissionChanged(ContentSettingsType::STORAGE_ACCESS,
                                       CONTENT_SETTING_ALLOW,
                                       url::Origin::Create(kEmbedded1),
                                       /*is_one_time=*/false);
  expected_visible_permissions.insert(ContentSettingsType::STORAGE_ACCESS);
#if !BUILDFLAG(IS_ANDROID)
  page_info()->OnSitePermissionChanged(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, CONTENT_SETTING_ALLOW,
      url::Origin::Create(kEmbedded1),
      /*is_one_time=*/false);
  expected_visible_permissions.insert(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  expected_visible_permissions.insert(ContentSettingsType::POPUPS);
  // Change a default-block setting to a user-preference block instead.
  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  expected_visible_permissions.insert(ContentSettingsType::JAVASCRIPT);
  // Change a default-allow setting away from the default.
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Make sure changing a setting to its default causes it to show up, since it
  // has been recently changed.
  expected_visible_permissions.insert(ContentSettingsType::MEDIASTREAM_CAMERA);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_DEFAULT,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Set the Javascript setting to default should keep it shown.
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_DEFAULT,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Change the default setting for Javascript away from the factory default.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  page_info()->PresentSitePermissionsForTesting();
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Change it back to ALLOW, which is its factory default, but has a source
  // from the user preference (i.e. it counts as non-factory default).
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Adding another storage access permission for a different embedded origin
  // adds an additional entry.
  page_info()->OnSitePermissionChanged(ContentSettingsType::STORAGE_ACCESS,
                                       CONTENT_SETTING_ALLOW,
                                       url::Origin::Create(kEmbedded2),
                                       /*is_one_time=*/false);
  EXPECT_EQ(expected_visible_permissions.size() + 1,
            last_permission_info_list().size());
}

TEST_F(PageInfoTest, StorageAccessGrantsAreFiltered) {
  GURL kEmbedded1("https://embedded1.com");
  ContentSettingsType type = ContentSettingsType::STORAGE_ACCESS;

  std::set<ContentSettingsType> expected_visible_permissions;

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  // First-party exceptions are hidden.
  map->SetContentSettingDefaultScope(url(), url(), type, CONTENT_SETTING_ALLOW);
  // First-party-set exceptions are hidden based on their SessionModel.
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  map->SetContentSettingDefaultScope(kEmbedded1, url(), type,
                                     CONTENT_SETTING_ALLOW, constraint);
  page_info()->PresentSitePermissionsForTesting();

#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  map->SetContentSettingDefaultScope(kEmbedded1, url(), type,
                                     CONTENT_SETTING_ALLOW);
  page_info()->PresentSitePermissionsForTesting();
  expected_visible_permissions.insert(type);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, StorageAccessGrantsDisplayedWhenDefaultBlocked) {
  GURL kEmbedded1("https://embedded1.com");
  ContentSettingsType type = ContentSettingsType::STORAGE_ACCESS;

  std::set<ContentSettingsType> expected_visible_permissions;

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  // Nothing is displayed for default permissions.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_BLOCK);
  page_info()->PresentSitePermissionsForTesting();

#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  // Until the permission is accessed and blocked.
  auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  pscs->OnTwoSitePermissionChanged(ContentSettingsType::STORAGE_ACCESS,
                                   net::SchemefulSite(kEmbedded1),
                                   CONTENT_SETTING_BLOCK);
  page_info()->PresentSitePermissionsForTesting();
  expected_visible_permissions.insert(type);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, ShowAutograntedRWSPermissions) {
  std::set<ContentSettingsType> expected_visible_permissions;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kShowRelatedWebsiteSetsPermissionGrants);
  SetURL("https://firstparty.com");
  constexpr char kToplevelURL[] = "https://firstparty.com";
  constexpr char kEmbeddedURL[] = "https://embedded.com";
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  map->SetContentSettingDefaultScope(GURL(kEmbeddedURL), GURL(kToplevelURL),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_BLOCK, constraint);
  page_info()->PresentSitePermissionsForTesting();
  expected_visible_permissions.insert(ContentSettingsType::STORAGE_ACCESS);
#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, HideAutograntedRWSPermissions) {
  std::set<ContentSettingsType> expected_visible_permissions;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::kShowRelatedWebsiteSetsPermissionGrants);
  SetURL("https://firstparty.com");
  constexpr char kToplevelURL[] = "https://firstparty.com";
  constexpr char kEmbeddedURL[] = "https://embedded.com";
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  map->SetContentSettingDefaultScope(GURL(kEmbeddedURL), GURL(kToplevelURL),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW, constraint);
  page_info()->PresentSitePermissionsForTesting();
#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, IncognitoPermissionsEmptyByDefault) {
  incognito_page_info()->PresentSitePermissionsForTesting();
  EXPECT_EQ(0u, last_permission_info_list().size());
}

TEST_F(PageInfoTest, IncognitoPermissionsDontShowAsk) {
  page_info()->PresentSitePermissionsForTesting();
  std::set<ContentSettingsType> expected_permissions;
  std::set<ContentSettingsType> expected_incognito_permissions;
#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_permissions, last_permission_info_list());

  // Add some permissions to regular page info.
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  expected_permissions.insert(ContentSettingsType::MEDIASTREAM_MIC);
  expected_incognito_permissions.insert(ContentSettingsType::MEDIASTREAM_MIC);

  // Both permissions should show in regular page info.
  EXPECT_EQ(2u, last_permission_info_list().size());

  // Only the block permissions should show in incognito mode as ALLOW
  // permissions are inherited as ASK.
  incognito_page_info()->PresentSitePermissionsForTesting();
  ExpectPermissionInfoList(expected_incognito_permissions,
                           last_permission_info_list());

  // Changing the permission to BLOCK should show it.
  incognito_page_info()->OnSitePermissionChanged(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK,
      /*requesting_origin=*/std::nullopt,
      /*is_one_time=*/false);
  expected_incognito_permissions.insert(ContentSettingsType::GEOLOCATION);
  ExpectPermissionInfoList(expected_incognito_permissions,
                           last_permission_info_list());

  // Switching a permission back to default should not hide the permission.
  incognito_page_info()->OnSitePermissionChanged(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_DEFAULT,
      /*requesting_origin=*/std::nullopt,
      /*is_one_time=*/false);
  ExpectPermissionInfoList(expected_incognito_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, OnPermissionsChanged) {
  GURL kEmbedded("https://embedded.com");

  // Setup site permissions.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::POPUPS);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_MIC);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_CAMERA);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      kEmbedded, url(), ContentSettingsType::STORAGE_ACCESS);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
#if !BUILDFLAG(IS_ANDROID)
  setting = content_settings->GetContentSetting(
      kEmbedded, url(), ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
#endif

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  ExpectInitialSetCookieInfoCall(mock_ui());

  // SetPermissionInfo() is called once initially, and then again every time
  // OnSitePermissionChanged() is called.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(8);
#else
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(7);
#endif

  // Execute code under tests.
  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::STORAGE_ACCESS,
                                       CONTENT_SETTING_ALLOW,
                                       url::Origin::Create(kEmbedded),
                                       /*is_one_time=*/false);
#if !BUILDFLAG(IS_ANDROID)
  page_info()->OnSitePermissionChanged(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD, CONTENT_SETTING_ALLOW,
      url::Origin::Create(kEmbedded),
      /*is_one_time=*/false);
#endif

  // Verify that the site permissions were changed correctly.
  setting = content_settings->GetContentSetting(url(), url(),
                                                ContentSettingsType::POPUPS);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_MIC);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_CAMERA);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      kEmbedded, url(), ContentSettingsType::STORAGE_ACCESS);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
#if !BUILDFLAG(IS_ANDROID)
  setting = content_settings->GetContentSetting(
      kEmbedded, url(), ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
#endif
}

TEST_F(PageInfoTest, OnChosenObjectDeleted) {
  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
  usb_device_manager.AddReceiver(
      device_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  store->SetDeviceManagerForTesting(std::move(device_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin(), *device_info);

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  ExpectInitialSetCookieInfoCall(mock_ui());

  // Access PageInfo so that SetPermissionInfo is called once to populate
  // |last_chosen_object_info_|. It will be called again by
  // OnSiteChosenObjectDeleted.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);
  page_info();

  ASSERT_EQ(1u, last_chosen_object_info().size());
  const PageInfoUI::ChosenObjectInfo* info = last_chosen_object_info()[0].get();
  page_info()->OnSiteChosenObjectDeleted(
      *info->ui_info, base::Value(info->chooser_object->value.Clone()));

  EXPECT_FALSE(store->HasDevicePermission(origin(), *device_info));
  EXPECT_EQ(0u, last_chosen_object_info().size());
}

TEST_F(PageInfoTest, Malware) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_MALWARE,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, ManagedPolicyBlock) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_BLOCK,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, ManagedPolicyWarn) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_MANAGED_POLICY_WARN,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, SocialEngineering) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, UnwantedSoftware) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE,
            page_info()->safe_browsing_status());
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST_F(PageInfoTest, SignInPasswordReuse) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, SavedPasswordReuse) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE,
            page_info()->safe_browsing_status());
}

TEST_F(PageInfoTest, EnterprisePasswordReuse) {
  security_level_ = security_state::DANGEROUS;
  visible_security_state_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE,
            page_info()->safe_browsing_status());
}
#endif

TEST_F(PageInfoTest, HTTPConnection) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_NO_CERT,
            page_info()->site_identity_status());
}

TEST_F(PageInfoTest, HTTPSConnection) {
  security_level_ = security_state::SECURE;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = 0;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
}

// Define some dummy constants for Android-only resources.
#if !BUILDFLAG(IS_ANDROID)
#define IDR_PAGEINFO_BAD 0
#define IDR_PAGEINFO_GOOD 0
#endif

TEST_F(PageInfoTest, InsecureContent) {
  struct TestCase {
    security_state::SecurityLevel security_level;
    net::CertStatus cert_status;
    bool ran_mixed_content;
    bool displayed_mixed_content;
    bool contained_mixed_form;
    bool ran_content_with_cert_errors;
    bool displayed_content_with_cert_errors;
    PageInfo::SiteConnectionStatus expected_site_connection_status;
    PageInfo::SiteIdentityStatus expected_site_identity_status;
    int expected_connection_icon_id;
  };

  const TestCase kTestCases[] = {
      // Passive mixed content.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content with a nonsecure form. The nonsecure form is the
      // more severe problem.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Only nonsecure form.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       false /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, true /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},
      // Active and passive mixed content.
      {security_state::DANGEROUS, 0, true /* ran_mixed_content */,
       true /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content and nonsecure form.
      {security_state::DANGEROUS, 0, true /* ran_mixed_content */,
       true /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active and passive mixed content with a cert error on the main
      // resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       true /* ran_mixed_content */, true /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},
      // Active mixed content.
      {security_state::DANGEROUS, 0, true /* ran_mixed_content */,
       false /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       true /* ran_mixed_content */, false /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},

      // Passive subresources with cert errors.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       false /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive subresources with cert errors, with a cert error on the
      // main resource also. In this case, the subresources with
      // certificate errors are ignored: if the main resource had a cert
      // error, it's not that useful to warn about subresources with cert
      // errors as well.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, false /* displayed_mixed_content */,
       false, false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Passive and active subresources with cert errors.
      {security_state::DANGEROUS, 0, false /* ran_mixed_content */,
       false /* displayed_mixed_content */, false,
       true /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive and active subresources with cert errors, with a cert
      // error on the main resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, false /* displayed_mixed_content */,
       false, true /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Active subresources with cert errors.
      {security_state::DANGEROUS, 0, false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */, false,
       true /* ran_mixed_content */, false /* displayed_mixed_content */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active subresources with cert errors, with a cert error on the main
      // resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, false /* displayed_mixed_content */,
       false, true /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},

      // Passive mixed content and subresources with cert errors.
      {security_state::NONE, 0, false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */, false,
       false /* ran_mixed_content */, true /* displayed_mixed_content */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content, a nonsecure form, and subresources with cert
      // errors.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content and active subresources with cert errors.
      {security_state::DANGEROUS, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, false,
       true /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content and passive subresources with cert errors.
      {security_state::DANGEROUS, 0, true /* ran_mixed_content */,
       false /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content, active subresources with cert errors, and a cert
      // error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, true /* displayed_mixed_content */, false,
       true /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},
  };

  for (const auto& test : kTestCases) {
    ClearPageInfo();
    ResetMockUI();
    security_level_ = test.security_level;
    visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
    visible_security_state_.certificate = cert();
    visible_security_state_.cert_status = test.cert_status;
    visible_security_state_.displayed_mixed_content =
        test.displayed_mixed_content;
    visible_security_state_.ran_mixed_content = test.ran_mixed_content;
    visible_security_state_.contained_mixed_form = test.contained_mixed_form;
    visible_security_state_.displayed_content_with_cert_errors =
        test.displayed_content_with_cert_errors;
    visible_security_state_.ran_content_with_cert_errors =
        test.ran_content_with_cert_errors;
    int status = 0;
    status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
    status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
    visible_security_state_.connection_status = status;
    visible_security_state_.connection_info_initialized = true;

    SetDefaultUIExpectations(mock_ui());

    EXPECT_EQ(test.expected_site_connection_status,
              page_info()->site_connection_status());
    EXPECT_EQ(test.expected_site_identity_status,
              page_info()->site_identity_status());
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(
        test.expected_connection_icon_id,
        PageInfoUI::GetConnectionIconID(page_info()->site_connection_status()));
#endif
  }
}

TEST_F(PageInfoTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(google_der);
  ASSERT_TRUE(ev_cert);

  security_level_ = security_state::NONE;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = ev_cert;
  visible_security_state_.cert_status = net::CERT_STATUS_IS_EV;
  visible_security_state_.displayed_mixed_content = true;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_EV_CERT,
            page_info()->site_identity_status());
}

TEST_F(PageInfoTest, HTTPSConnectionError) {
  security_level_ = security_state::SECURE;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = 0;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;

  // Simulate a failed connection.
  visible_security_state_.connection_info_initialized = false;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PageInfoTest, HTTPSPolicyCertConnection) {
  security_level_ = security_state::SECURE_WITH_POLICY_INSTALLED_CERT;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = 0;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT,
            page_info()->site_identity_status());
}
#endif

TEST_F(PageInfoTest, HTTPSSHA1) {
  SetCertToSHA1();
  security_level_ = security_state::NONE;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = net::CERT_STATUS_SHA1_SIGNATURE_PRESENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM,
            page_info()->site_identity_status());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(IDR_PAGEINFO_BAD,
            PageInfoUI::GetIdentityIconID(page_info()->site_identity_status()));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageInfoTest, NoInfoBar) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(0u, infobar_manager()->infobars().size());
}

TEST_F(PageInfoTest, ShowInfoBar) {
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  ExpectInitialSetCookieInfoCall(mock_ui());

  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);

  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  bool unused;
  page_info()->OnUIClosing(&unused);
  ASSERT_EQ(1u, infobar_manager()->infobars().size());

  infobar_manager()->RemoveInfoBar(infobar_manager()->infobars()[0]);
}

TEST_F(PageInfoTest, NoInfoBarWhenSoundSettingChanged) {
  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  page_info()->OnSitePermissionChanged(
      ContentSettingsType::SOUND, CONTENT_SETTING_BLOCK,
      /*requesting_origin=*/std::nullopt, /*is_one_time=*/false);
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(0u, infobar_manager()->infobars().size());
}

TEST_F(PageInfoTest, ShowInfoBarWhenSoundSettingAndAnotherSettingChanged) {
  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(
      ContentSettingsType::SOUND, CONTENT_SETTING_BLOCK,
      /*requesting_origin=*/std::nullopt, /*is_one_time=*/false);
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());

  infobar_manager()->RemoveInfoBar(infobar_manager()->infobars()[0]);
}

TEST_F(PageInfoTest, ShowInfobarWhenMediaChanged) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     CONTENT_SETTING_BLOCK);

  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.VideoCapture.ReloadInfobarShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.AudioCapture.ReloadInfobarShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
}

// Camera and Mic permissions should suppress the infobar when changed to BLOCK.
TEST_F(PageInfoTest, SuppressInfobarWhenMediaChangedToBlock) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     CONTENT_SETTING_BLOCK);

  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(0u, infobar_manager()->infobars().size());

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.VideoCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.AudioCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
}

TEST_F(PageInfoTest, SuppressInfobarWhenMediaChangedToAllow) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     CONTENT_SETTING_BLOCK);

  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(0u, infobar_manager()->infobars().size());

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.VideoCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.AudioCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
}

// Camera and Mic permissions should suppress the infobar when changed to
// DEFAULT.
TEST_F(PageInfoTest, SuppressInfobarWhenMediaChangedToDefault) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     CONTENT_SETTING_BLOCK);

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_DEFAULT,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_DEFAULT,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(0u, infobar_manager()->infobars().size());

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.VideoCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::RESET_FROM_DENIED, 1);

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.AudioCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::RESET_FROM_DENIED, 1);
}

// Only Camera and Mic permissions can suppress the infobar.
TEST_F(PageInfoTest, ShowInfobarWhenGeolocationChangedToAllow) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.Geolocation.ReloadInfobarShown",
      permissions::PermissionChangeAction::REALLOWED, 0);
}

// Geolocation permission change to BLOCK should show the infobar. Only
// Camera & Mic should suppress the infobar when changed.
TEST_F(PageInfoTest, NotSuppressedInfobarWhenGeolocationChangedToBlock) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.Geolocation.ReloadInfobarShown",
      permissions::PermissionChangeAction::REVOKED, 0);
}

TEST_F(PageInfoTest, ShowInfobarWhenGeolocationChangedToDefault) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_DEFAULT,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.Geolocation.ReloadInfobarShown",
      permissions::PermissionChangeAction::RESET_FROM_ALLOWED, 0);
}

// If at least one permission requires to show the infobar, we show it.
TEST_F(PageInfoTest, ShowInfobarWhenGeolocationAndMediaChangedToBlock) {
  base::HistogramTester histograms;

  ASSERT_EQ(0u, infobar_manager()->infobars().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(url(), url(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     CONTENT_SETTING_BLOCK);

  // The infobar can be suppressed only if an origin subscribed to permission
  // status change.
  page_info()->SetSubscribedToPermissionChangeForTesting();

  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  page_info()->OnUIClosing(nullptr);
  EXPECT_EQ(1u, infobar_manager()->infobars().size());

  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.Geolocation.ReloadInfobarShown",
      permissions::PermissionChangeAction::REVOKED, 0);
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.VideoCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
  histograms.ExpectUniqueSample(
      "Permissions.PageInfo.Changed.AudioCapture.ReloadInfobarNotShown",
      permissions::PermissionChangeAction::REALLOWED, 1);
}

TEST_F(PageInfoTest, ShowInfoBarWhenAllowingThirdPartyCookies) {
  SetDefaultUIExpectations(mock_ui());
  NavigateAndCommit(url());

  // Calls to `PresentSiteDataInternal` from `PresentSiteData` are synchronous
  // which makes calls to `SetCookieInfo` appear as they're called.
  // This call is needed to satisfy the default expectations after navigation.
  page_info();
  Mock::VerifyAndClearExpectations(mock_ui());
  // `SetCookieInfo` is called once through `OnStatusChanged` and another time
  // through `OnThirdPartyToggleClicked` which calls `OnStatusChanged` down
  // its call chain.
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_)).Times(2);

  page_info()->OnStatusChanged(
      /*controls_visible=*/true, /*protections_on=*/true,
      CookieControlsEnforcement::kNoEnforcement,
      CookieBlocking3pcdStatus::kNotIn3pcd, base::Time(), /*features=*/{});

  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  page_info()->OnThirdPartyToggleClicked(/*block_third_party_cookies=*/false);
  page_info()->OnUIClosing(nullptr);
  ASSERT_EQ(1u, infobar_manager()->infobars().size());

  infobar_manager()->RemoveInfoBar(infobar_manager()->infobars()[0]);
}

TEST_F(PageInfoTest, ShowInfoBarWhenBlockingThirdPartyCookies) {
  SetDefaultUIExpectations(mock_ui());
  NavigateAndCommit(url());

  // As in `ShowInfoBarWhenAllowingThirdPartyCookies` above, expectations need
  // to be cleared.
  page_info();
  Mock::VerifyAndClearExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_)).Times(2);

  page_info()->OnStatusChanged(
      /*controls_visible=*/true, /*protections_on=*/false,
      CookieControlsEnforcement::kNoEnforcement,
      CookieBlocking3pcdStatus::kNotIn3pcd, base::Time(), /*features=*/{});

  EXPECT_EQ(0u, infobar_manager()->infobars().size());
  page_info()->OnThirdPartyToggleClicked(/*block_third_party_cookies=*/true);
  page_info()->OnUIClosing(nullptr);
  ASSERT_EQ(1u, infobar_manager()->infobars().size());

  infobar_manager()->RemoveInfoBar(infobar_manager()->infobars()[0]);
}

#endif

TEST_F(PageInfoTest, AboutBlankPage) {
  SetURL(url::kAboutBlankURL);
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_NO_CERT,
            page_info()->site_identity_status());
}

// On desktop, internal URLs aren't handled by PageInfo class. Instead, a
// custom and simpler bubble is shown, so no need to test.
#if BUILDFLAG(IS_ANDROID)
TEST_F(PageInfoTest, InternalPage) {
  SetURL("chrome://bookmarks");
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_INTERNAL_PAGE,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE,
            page_info()->site_identity_status());
}
#endif

// Tests that "Re-Enable Warnings" button on PageInfo both removes certificate
// exceptions and logs metrics correctly.
TEST_F(PageInfoTest, ReEnableWarnings) {
  struct TestCase {
    const std::string url;
    const bool button_visible;
    const bool button_clicked;
  };

  const TestCase kTestCases[] = {
      {"https://example.test", false, false},
      {"https://example.test", true, false},
      {"https://example.test", true, true},
  };
  const char kGenericHistogram[] =
      "interstitial.ssl.did_user_revoke_decisions2";
  auto* storage_partition =
      web_contents()->GetPrimaryMainFrame()->GetStoragePartition();
  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    StatefulSSLHostStateDelegate* ssl_state =
        StatefulSSLHostStateDelegateFactory::GetForProfile(profile());
    const std::string host = GURL(test.url).host();

    ssl_state->RevokeUserAllowExceptionsHard(host);
    ResetMockUI();
    SetURL(test.url);
    if (test.button_visible) {
      // In the case where the button should be visible, add an exception to
      // the profile settings for the site (since the exception is what
      // will make the button visible).
      ssl_state->AllowCert(host, *cert(), net::ERR_CERT_DATE_INVALID,
                           storage_partition);
      page_info();
      if (test.button_clicked) {
        page_info()->OnRevokeSSLErrorBypassButtonPressed();
        EXPECT_FALSE(ssl_state->HasAllowException(host, storage_partition));
        ClearPageInfo();
        histograms.ExpectTotalCount(kGenericHistogram, 1);
        histograms.ExpectBucketCount(
            kGenericHistogram,
            PageInfo::SSLCertificateDecisionsDidRevoke::
                USER_CERT_DECISIONS_REVOKED,
            1);
      } else {  // Case where button is visible but not clicked.
        ClearPageInfo();
        EXPECT_TRUE(ssl_state->HasAllowException(host, storage_partition));
        histograms.ExpectTotalCount(kGenericHistogram, 1);
        histograms.ExpectBucketCount(
            kGenericHistogram,
            PageInfo::SSLCertificateDecisionsDidRevoke::
                USER_CERT_DECISIONS_NOT_REVOKED,
            1);
      }
    } else {
      page_info();
      ClearPageInfo();
      EXPECT_FALSE(ssl_state->HasAllowException(host, storage_partition));
      // Button is not visible, so check histogram is empty after opening and
      // closing page info.
      histograms.ExpectTotalCount(kGenericHistogram, 0);
    }
  }
  // Test class expects PageInfo to exist during Teardown.
  page_info();
}

// Tests that the duration of time the PageInfo is open is recorded for pages
// with various security levels.
TEST_F(PageInfoTest, TimeOpenMetrics) {
  struct TestCase {
    const std::string url;
    const security_state::SecurityLevel security_level;
    const std::string security_level_name;
    const page_info::PageInfoAction action;
  };

  const std::string kHistogramPrefix("Security.PageInfo.TimeOpen.");

  const TestCase kTestCases[] = {
      // PAGE_INFO_OPENED used as shorthand for "take no action".
      {"https://example.test", security_state::SECURE, "SECURE",
       page_info::PAGE_INFO_OPENED},
      {"http://example.test", security_state::NONE, "NONE",
       page_info::PAGE_INFO_OPENED},
      {"https://example.test", security_state::SECURE, "SECURE",
       page_info::PAGE_INFO_SITE_SETTINGS_OPENED},
      {"http://example.test", security_state::NONE, "NONE",
       page_info::PAGE_INFO_SITE_SETTINGS_OPENED},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL(test.url);
    security_level_ = test.security_level;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kHistogramPrefix + test.security_level_name, 0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "Action." + test.security_level_name, 0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "NoAction." + test.security_level_name, 0);

    PageInfo* test_page_info = page_info();
    if (test.action != page_info::PAGE_INFO_OPENED) {
      test_page_info->RecordPageInfoAction(test.action);
    }
    ClearPageInfo();

    histograms.ExpectTotalCount(kHistogramPrefix + test.security_level_name, 1);

    if (test.action != page_info::PAGE_INFO_OPENED) {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "Action." + test.security_level_name, 1);
    } else {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "NoAction." + test.security_level_name, 1);
    }
  }

  // PageInfoTest expects a valid PageInfo instance to exist at end of test.
  ResetMockUI();
  SetDefaultUIExpectations(mock_ui());
  page_info();
}

TEST_F(PageInfoTest, AdPersonalization) {
  constexpr int kTaxonomyVersion = 1;
  privacy_sandbox::CanonicalTopic kFirstTopic(
      browsing_topics::Topic(24),  // "Blues"
      kTaxonomyVersion);
  privacy_sandbox::CanonicalTopic kSecondTopic(
      browsing_topics::Topic(23),  // "Music & audio"
      kTaxonomyVersion);

  std::vector<privacy_sandbox::CanonicalTopic> accessed_topics = {kFirstTopic,
                                                                  kSecondTopic};
  EXPECT_CALL(*mock_ui(),
              SetAdPersonalizationInfo(::testing::Field(
                  &PageInfoUI::AdPersonalizationInfo::accessed_topics,
                  accessed_topics)));

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(pscs->HasAccessedTopics());
  EXPECT_THAT(pscs->GetAccessedTopics(), testing::IsEmpty());

  pscs->OnTopicAccessed(url::Origin::Create(GURL("https://foo.com")), false,
                        kSecondTopic);
  pscs->OnTopicAccessed(url::Origin::Create(GURL("https://foo.com")), false,
                        kFirstTopic);
  page_info();
}

// Tests that metrics are recorded on a PageInfo for pages with
// various Safety Tip statuses.
// See https://crbug.com/1114659 for why the test is disabled on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SafetyTipMetrics DISABLED_SafetyTipMetrics
#else
#define MAYBE_SafetyTipMetrics SafetyTipMetrics
#endif
TEST_F(PageInfoTest, MAYBE_SafetyTipMetrics) {
  struct TestCase {
    const security_state::SafetyTipInfo safety_tip_info;
  };
  const char kGenericHistogram[] = "WebsiteSettings.Action";

  const TestCase kTestCases[] = {
      {{security_state::SafetyTipStatus::kNone, GURL()}},
      {{security_state::SafetyTipStatus::kLookalike, GURL()}},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL("https://example.test");
    visible_security_state_.safety_tip_info = test.safety_tip_info;
    ClearPageInfo();
    ResetMockUI();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);

    page_info()->RecordPageInfoAction(page_info::PAGE_INFO_OPENED);

    // RecordPageInfoAction() is called during PageInfo
    // creation in addition to the explicit RecordPageInfoAction()
    // call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(kGenericHistogram, page_info::PAGE_INFO_OPENED,
                                 2);
  }
}

// Tests that the duration of time the PageInfo is open is recorded for pages
// with various Safety Tip statuses.
TEST_F(PageInfoTest, SafetyTipTimeOpenMetrics) {
  struct TestCase {
    const security_state::SafetyTipStatus safety_tip_status;
    const std::string safety_tip_status_name;
    const page_info::PageInfoAction action;
  };

  const std::string kHistogramPrefix("Security.PageInfo.TimeOpen.");

  const TestCase kTestCases[] = {
      // PAGE_INFO_COUNT used as shorthand for "take no action".
      {security_state::SafetyTipStatus::kNone, "SafetyTip_None",
       page_info::PAGE_INFO_OPENED},
      {security_state::SafetyTipStatus::kLookalike, "SafetyTip_Lookalike",
       page_info::PAGE_INFO_OPENED},
      {security_state::SafetyTipStatus::kNone, "SafetyTip_None",
       page_info::PAGE_INFO_SITE_SETTINGS_OPENED},
      {security_state::SafetyTipStatus::kLookalike, "SafetyTip_Lookalike",
       page_info::PAGE_INFO_SITE_SETTINGS_OPENED},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL("https://example.test");
    visible_security_state_.safety_tip_info.status = test.safety_tip_status;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kHistogramPrefix + test.safety_tip_status_name,
                                0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "Action." + test.safety_tip_status_name, 0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "NoAction." + test.safety_tip_status_name, 0);

    PageInfo* test_page_info = page_info();
    if (test.action != page_info::PAGE_INFO_OPENED) {
      test_page_info->RecordPageInfoAction(test.action);
    }
    ClearPageInfo();

    histograms.ExpectTotalCount(kHistogramPrefix + test.safety_tip_status_name,
                                1);

    if (test.action != page_info::PAGE_INFO_OPENED) {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "Action." + test.safety_tip_status_name, 1);
    } else {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "NoAction." + test.safety_tip_status_name, 1);
    }
  }

  // PageInfoTest expects a valid PageInfo instance to exist at end of test.
  ResetMockUI();
  SetDefaultUIExpectations(mock_ui());
  page_info();
}

// Tests that the SubresourceFilter setting is omitted correctly.
TEST_F(PageInfoTest, SubresourceFilterSetting_MatchesActivation) {
  auto showing_setting = [](const PermissionInfoList& permissions) {
    return base::Contains(
        permissions, ContentSettingsType::ADS,
        [](const auto& permission) { return permission.type; });
  };

  // By default, the setting should not appear at all.
  SetURL("https://example.test/");
  SetDefaultUIExpectations(mock_ui());
  page_info();
  EXPECT_FALSE(showing_setting(last_permission_info_list()));

  // Reset state.
  ClearPageInfo();
  ResetMockUI();
  SetDefaultUIExpectations(mock_ui());

  // Now, explicitly set site activation metadata to simulate activation on
  // that origin, which is encoded by the existence of the website setting. The
  // setting should then appear in page_info.
  subresource_filter::SubresourceFilterContentSettingsManager*
      settings_manager =
          SubresourceFilterProfileContextFactory::GetForProfile(profile())
              ->settings_manager();
  settings_manager->SetSiteMetadataBasedOnActivation(
      url(), true,
      subresource_filter::SubresourceFilterContentSettingsManager::
          ActivationSource::kSafeBrowsing);

  page_info();
  EXPECT_TRUE(showing_setting(last_permission_info_list()));
}

// Tests that permissions substring is empty if permission is blocked.
TEST_F(PageInfoTest, PermissionBlockedStrings) {
  SetURL("https://example.com/");
  page_info();

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ChromePageInfoUiDelegate delegate(web_contents.get(),
                                    GURL("http://www.example.com"));

  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_BLOCK;
  camera_permission.default_setting = CONTENT_SETTING_ASK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;

  EXPECT_EQ(std::u16string(), PageInfoUI::PermissionMainPageStateToUIString(
                                  &delegate, camera_permission));
}

// Tests that permissions substring says "Using now" if permission is in use.
TEST_F(PageInfoTest, PermissionUsingNowStrings) {
  SetURL("https://example.com/");
  page_info();

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ChromePageInfoUiDelegate delegate(web_contents.get(),
                                    GURL("http://www.example.com"));

  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_ALLOW;
  camera_permission.default_setting = CONTENT_SETTING_ASK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;
  camera_permission.is_in_use = true;

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_USING_NOW),
            PageInfoUI::PermissionMainPageStateToUIString(&delegate,
                                                          camera_permission));
}

// Tests that permissions substring says "Recently used" if permission was used
// less than 1 minute ago.
TEST_F(PageInfoTest, PermissionRecentlyUsedStrings) {
  SetURL("https://example.com/");
  page_info();

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ChromePageInfoUiDelegate delegate(web_contents.get(),
                                    GURL("http://www.example.com"));

  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_ALLOW;
  camera_permission.default_setting = CONTENT_SETTING_ASK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;
  camera_permission.is_in_use = false;
  camera_permission.last_used = base::Time::Now() - base::Seconds(30);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_RECENTLY_USED),
            PageInfoUI::PermissionMainPageStateToUIString(&delegate,
                                                          camera_permission));
}

// Tests that permissions substring says "Used X minutes/hours ago" if
// permission was used more than 1 minute ago.
TEST_F(PageInfoTest, PermissionUsed30MinutesAgoStrings) {
  SetURL("https://example.com/");
  page_info();

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ChromePageInfoUiDelegate delegate(web_contents.get(),
                                    GURL("http://www.example.com"));

  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_ALLOW;
  camera_permission.default_setting = CONTENT_SETTING_ASK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;
  camera_permission.is_in_use = false;
  camera_permission.last_used = base::Time::Now() - base::Minutes(30);

  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_PAGE_INFO_PERMISSION_USED_TIME_AGO,
                                       u"30 minutes"),
            PageInfoUI::PermissionMainPageStateToUIString(&delegate,
                                                          camera_permission));
}

#if !BUILDFLAG(IS_ANDROID)

// Unit tests with the unified autoplay sound settings UI enabled. When enabled
// the sound settings dropdown on the page info UI will have custom wording.

class UnifiedAutoplaySoundSettingsPageInfoTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~UnifiedAutoplaySoundSettingsPageInfoTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({media::kAutoplayDisableSettings},
                                          {});
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void SetAutoplayPrefValue(bool value) {
    profile()->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, value);
  }

  void SetDefaultSoundContentSetting(ContentSetting default_setting) {
    default_setting_ = default_setting;
  }

  std::u16string GetDefaultSoundSettingString() {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    ChromePageInfoUiDelegate delegate(web_contents.get(),
                                      GURL("http://www.example.com"));
    return PageInfoUI::PermissionActionToUIString(
        &delegate, ContentSettingsType::SOUND, CONTENT_SETTING_DEFAULT,
        default_setting_, SettingSource::kUser,
        /*is_one_time=*/false);
  }

  std::u16string GetSoundSettingString(ContentSetting setting) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    ChromePageInfoUiDelegate delegate(web_contents.get(),
                                      GURL("http://www.example.com"));
    return PageInfoUI::PermissionActionToUIString(
        &delegate, ContentSettingsType::SOUND, setting, default_setting_,
        SettingSource::kUser,
        /*is_one_time=*/false);
  }

 private:
  ContentSetting default_setting_ = CONTENT_SETTING_DEFAULT;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that the strings for the sound settings dropdown when
// block autoplay is enabled and the default sound setting is allow.
// The three options should be Automatic (default), Allow and Mute.
TEST_F(UnifiedAutoplaySoundSettingsPageInfoTest, DefaultAllow_PrefOn) {
  SetDefaultSoundContentSetting(CONTENT_SETTING_ALLOW);
  SetAutoplayPrefValue(true);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_AUTOMATIC_BY_DEFAULT),
      GetDefaultSoundSettingString());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER),
      GetSoundSettingString(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_USER),
            GetSoundSettingString(CONTENT_SETTING_BLOCK));
}

// This test checks that the strings for the sound settings dropdown when
// block autoplay is disabled and the default sound setting is allow.
// The three options should be Allow (default), Allow and Mute.
TEST_F(UnifiedAutoplaySoundSettingsPageInfoTest, DefaultAllow_PrefOff) {
  SetDefaultSoundContentSetting(CONTENT_SETTING_ALLOW);
  SetAutoplayPrefValue(false);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_DEFAULT),
      GetDefaultSoundSettingString());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER),
      GetSoundSettingString(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_USER),
            GetSoundSettingString(CONTENT_SETTING_BLOCK));
}

// This test checks the strings for the sound settings dropdown when
// the default sound setting is block. The three options should be
// Block (default), Allow and Mute.
TEST_F(UnifiedAutoplaySoundSettingsPageInfoTest, DefaultBlock_PrefOn) {
  SetDefaultSoundContentSetting(CONTENT_SETTING_BLOCK);
  SetAutoplayPrefValue(true);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_DEFAULT),
      GetDefaultSoundSettingString());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER),
      GetSoundSettingString(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_USER),
            GetSoundSettingString(CONTENT_SETTING_BLOCK));
}

// This test checks the strings for the sound settings dropdown when
// the default sound setting is block. The three options should be
// Block (default), Allow and Mute.
TEST_F(UnifiedAutoplaySoundSettingsPageInfoTest, DefaultBlock_PrefOff) {
  SetDefaultSoundContentSetting(CONTENT_SETTING_BLOCK);
  SetAutoplayPrefValue(false);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_DEFAULT),
      GetDefaultSoundSettingString());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_USER),
      GetSoundSettingString(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_MUTED_BY_USER),
            GetSoundSettingString(CONTENT_SETTING_BLOCK));
}

// This test checks that the string for a permission dropdown that is not the
// sound setting is unaffected.
TEST_F(UnifiedAutoplaySoundSettingsPageInfoTest, NotSoundSetting_Noop) {
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ChromePageInfoUiDelegate delegate(web_contents.get(),
                                    GURL("http://www.example.com"));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_DEFAULT),
      PageInfoUI::PermissionActionToUIString(
          &delegate, ContentSettingsType::ADS, CONTENT_SETTING_DEFAULT,
          CONTENT_SETTING_ALLOW, SettingSource::kUser,
          /*is_one_time=*/false));
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Unit tests for logic in the PageInfoUI that toggles permission between
// allow/block and remember/forget.
class PageInfoToggleStatesUnitTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kOneTimePermission);
    ::testing::Test::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Testing all possible state transitions for a permission that doesn't
// support allow once.
TEST_F(PageInfoToggleStatesUnitTest,
       TogglePermissionWithoutAllowOnceDefaultAskTest) {
  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_ALLOW;
  camera_permission.default_setting = CONTENT_SETTING_ASK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_ALLOW);
}

TEST_F(PageInfoToggleStatesUnitTest,
       TogglePermissionWithoutAllowOnceDefaultBlockTest) {
  PageInfo::PermissionInfo camera_permission;
  camera_permission.type = ContentSettingsType::MEDIASTREAM_CAMERA;
  camera_permission.setting = CONTENT_SETTING_ALLOW;
  camera_permission.default_setting = CONTENT_SETTING_BLOCK;
  camera_permission.source = SettingSource::kUser;
  camera_permission.is_one_time = false;

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_ALLOW);

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_BLOCK);

  // Block (default) -> Allow
  camera_permission.setting = CONTENT_SETTING_DEFAULT;
  PageInfoUI::ToggleBetweenAllowAndBlock(camera_permission);
  EXPECT_EQ(camera_permission.setting, CONTENT_SETTING_ALLOW);
}

// Testing all possible state transitions for a permission that supports
// allow once and default setting ask.
TEST_F(PageInfoToggleStatesUnitTest,
       TogglePermissionWithAllowOnceDefaultAskTest) {
  PageInfo::PermissionInfo location_permission;
  location_permission.type = ContentSettingsType::GEOLOCATION;
  location_permission.setting = CONTENT_SETTING_ALLOW;
  location_permission.default_setting = CONTENT_SETTING_ASK;
  location_permission.source = SettingSource::kUser;
  location_permission.is_one_time = false;

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);

  // Allow -> Allow once
  PageInfoUI::ToggleBetweenRememberAndForget(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, true);

  // Allow once -> Allow
  PageInfoUI::ToggleBetweenRememberAndForget(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, false);
}

// Testing all possible state transitions for a permission that supports
// allow once and default setting block.
TEST_F(PageInfoToggleStatesUnitTest,
       TogglePermissionWithAllowOnceDefaultBlockTest) {
  PageInfo::PermissionInfo location_permission;
  location_permission.type = ContentSettingsType::GEOLOCATION;
  location_permission.setting = CONTENT_SETTING_DEFAULT;
  location_permission.default_setting = CONTENT_SETTING_BLOCK;
  location_permission.source = SettingSource::kUser;
  location_permission.is_one_time = false;

  // Block (default) -> Allow once
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, true);

  // Allow once -> Allow
  PageInfoUI::ToggleBetweenRememberAndForget(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, false);

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, false);

  // Allow -> Allow once
  PageInfoUI::ToggleBetweenRememberAndForget(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(location_permission.is_one_time, true);

  // Allow once -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(location_permission);
  EXPECT_EQ(location_permission.setting, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(location_permission.is_one_time, false);
}

// Testing all possible state transitions for a content settings with a default
// setting allow.
TEST_F(PageInfoToggleStatesUnitTest, TogglePermissionDefaultAllowTest) {
  PageInfo::PermissionInfo images_permission;
  images_permission.type = ContentSettingsType::IMAGES;
  images_permission.setting = CONTENT_SETTING_DEFAULT;
  images_permission.default_setting = CONTENT_SETTING_ALLOW;
  images_permission.source = SettingSource::kUser;
  images_permission.is_one_time = false;

  // Allow (default) -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(images_permission);
  EXPECT_EQ(images_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(images_permission);
  EXPECT_EQ(images_permission.setting, CONTENT_SETTING_ALLOW);

  // Allow -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(images_permission);
  EXPECT_EQ(images_permission.setting, CONTENT_SETTING_BLOCK);
}

// Testing all possible state transitions for a content settings with a default
// setting block.
TEST_F(PageInfoToggleStatesUnitTest, TogglePermissionDefaultBlockTest) {
  PageInfo::PermissionInfo popups_permission;
  popups_permission.type = ContentSettingsType::POPUPS;
  popups_permission.setting = CONTENT_SETTING_DEFAULT;
  popups_permission.default_setting = CONTENT_SETTING_BLOCK;
  popups_permission.source = SettingSource::kUser;
  popups_permission.is_one_time = false;

  // Block (default) -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(popups_permission);
  EXPECT_EQ(popups_permission.setting, CONTENT_SETTING_ALLOW);

  // Allow -> Block
  // The page info always creates a site exception even if the target setting
  // matches the default.
  PageInfoUI::ToggleBetweenAllowAndBlock(popups_permission);
  EXPECT_EQ(popups_permission.setting, CONTENT_SETTING_BLOCK);

  // Block -> Allow
  PageInfoUI::ToggleBetweenAllowAndBlock(popups_permission);
  EXPECT_EQ(popups_permission.setting, CONTENT_SETTING_ALLOW);
}

// Testing all possible state transitions for a guard content settings with a
// default setting ask.
TEST_F(PageInfoToggleStatesUnitTest, ToggleGuardPermissionDefaultAskTest) {
  PageInfo::PermissionInfo usb_guard;
  usb_guard.type = ContentSettingsType::USB_GUARD;
  usb_guard.setting = CONTENT_SETTING_DEFAULT;
  usb_guard.default_setting = CONTENT_SETTING_ASK;
  usb_guard.source = SettingSource::kUser;
  usb_guard.is_one_time = false;

  // Ask (default) -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(usb_guard);
  EXPECT_EQ(usb_guard.setting, CONTENT_SETTING_BLOCK);

  // Block -> Ask
  // The page info always creates a site exception even if the target setting
  // matches the default.
  PageInfoUI::ToggleBetweenAllowAndBlock(usb_guard);
  EXPECT_EQ(usb_guard.setting, CONTENT_SETTING_ASK);

  // Ask -> Block
  PageInfoUI::ToggleBetweenAllowAndBlock(usb_guard);
  EXPECT_EQ(usb_guard.setting, CONTENT_SETTING_BLOCK);
}

// Testing all possible state transitions for a guard content settings with a
// default setting block.
TEST_F(PageInfoToggleStatesUnitTest, ToggleGuardPermissionDefaultBlockTest) {
  PageInfo::PermissionInfo hid_guard;
  hid_guard.type = ContentSettingsType::HID_GUARD;
  hid_guard.setting = CONTENT_SETTING_DEFAULT;
  hid_guard.default_setting = CONTENT_SETTING_BLOCK;
  hid_guard.source = SettingSource::kUser;
  hid_guard.is_one_time = false;

  // Block (default) -> Ask
  PageInfoUI::ToggleBetweenAllowAndBlock(hid_guard);
  EXPECT_EQ(hid_guard.setting, CONTENT_SETTING_ASK);

  // Ask -> Block
  // The page info always creates a site exception even if the target setting
  // matches the default.
  PageInfoUI::ToggleBetweenAllowAndBlock(hid_guard);
  EXPECT_EQ(hid_guard.setting, CONTENT_SETTING_BLOCK);

  // Block -> Ask
  PageInfoUI::ToggleBetweenAllowAndBlock(hid_guard);
  EXPECT_EQ(hid_guard.setting, CONTENT_SETTING_ASK);
}

TEST_F(PageInfoTest, WithoutPageSpecificContentSettings) {
  SetContents(CreateTestWebContents());
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage()));
  page_info();
}

TEST_F(PageInfoTest, MidiGrantsAreFilteredWhenAllowSysex) {
  std::set<ContentSettingsType> expected_visible_permissions;

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  page_info()->PresentSitePermissionsForTesting();

#if BUILDFLAG(IS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());

  map->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  page_info()->PresentSitePermissionsForTesting();
  expected_visible_permissions.insert(ContentSettingsType::MIDI_SYSEX);
  ExpectPermissionInfoList(expected_visible_permissions,
                           last_permission_info_list());
}

TEST_F(PageInfoTest, OriginLevelExceptionScope) {
  SetURL("https://www.example.com");

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  page_info()->PresentSitePermissionsForTesting();
  map->SetContentSettingDefaultScope(url(), url(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_ALLOW);

  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  // Page info has created an allow origin-level exception.
  content_settings::SettingInfo info;
  ContentSetting setting =
      map->GetContentSetting(url(), url(), ContentSettingsType::POPUPS, &info);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(info.primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(url()));
  EXPECT_EQ(info.secondary_pattern, ContentSettingsPattern::Wildcard());

  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  // Page info has created a block origin-level exception.
  setting =
      map->GetContentSetting(url(), url(), ContentSettingsType::POPUPS, &info);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(info.primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(url()));
  EXPECT_EQ(info.secondary_pattern, ContentSettingsPattern::Wildcard());
}

TEST_F(PageInfoTest, CustomExceptionScope) {
  SetURL("https://www.example.com");

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  page_info()->PresentSitePermissionsForTesting();
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://*"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
      CONTENT_SETTING_BLOCK);

  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_ALLOW,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  // Page info has created an allow origin-level exception.
  content_settings::SettingInfo info;
  ContentSetting setting = map->GetContentSetting(
      url(), url(), ContentSettingsType::JAVASCRIPT, &info);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(info.primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(url()));
  EXPECT_EQ(info.secondary_pattern, ContentSettingsPattern::Wildcard());

  // Other origins matching the pattern are not affected.
  GURL another_url = GURL("https://www.another.com");
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(another_url, another_url,
                                   ContentSettingsType::JAVASCRIPT));

  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK,
                                       /*requesting_origin=*/std::nullopt,
                                       /*is_one_time=*/false);

  // Page info has created a block origin-level exception.
  setting = map->GetContentSetting(url(), url(),
                                   ContentSettingsType::JAVASCRIPT, &info);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(info.primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(url()));
  EXPECT_EQ(info.secondary_pattern, ContentSettingsPattern::Wildcard());
}
