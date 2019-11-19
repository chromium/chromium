// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_state/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#endif

using content::SSLStatus;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
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
  ~MockPageInfoUI() override {}
  MOCK_METHOD1(SetCookieInfo, void(const CookieInfoList& cookie_info_list));
  MOCK_METHOD0(SetPermissionInfoStub, void());
  MOCK_METHOD1(SetIdentityInfo, void(const IdentityInfo& identity_info));
  MOCK_METHOD1(SetPageFeatureInfo, void(const PageFeatureInfo& info));

  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list,
      ChosenObjectInfoList chosen_object_info_list) override {
    SetPermissionInfoStub();
    if (set_permission_info_callback_) {
      set_permission_info_callback_.Run(permission_info_list,
                                        std::move(chosen_object_info_list));
    }
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  std::unique_ptr<PageInfoUI::SecurityDescription>
  CreateSecurityDescriptionForPasswordReuse() const override {
    std::unique_ptr<PageInfoUI::SecurityDescription> security_description(
        new PageInfoUI::SecurityDescription());
    security_description->summary_style = SecuritySummaryColor::RED;
    security_description->summary =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
    security_description->details =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS);
    security_description->type = SecurityDescriptionType::SAFE_BROWSING;
    return security_description;
  }
#endif

  base::Callback<void(const PermissionInfoList& permission_info_list,
                      ChosenObjectInfoList chosen_object_info_list)>
      set_permission_info_callback_;
};

class PageInfoTest : public ChromeRenderViewHostTestHarness {
 public:
  PageInfoTest() { SetURL("http://www.example.com"); }

  ~PageInfoTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Setup stub security info.
    security_level_ = security_state::NONE;

    // Create the certificate.
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);

    TabSpecificContentSettings::CreateForWebContents(web_contents());
    MockInfoBarService::CreateForWebContents(web_contents());

    // Setup mock ui.
    ResetMockUI();
  }

  void TearDown() override {
    ASSERT_TRUE(page_info_.get()) << "No PageInfo instance created.";
    RenderViewHostTestHarness::TearDown();
    page_info_.reset();
  }

  void SetDefaultUIExpectations(MockPageInfoUI* mock_ui) {
    // During creation |PageInfo| makes the following calls to the ui.
    EXPECT_CALL(*mock_ui, SetPermissionInfoStub());
    EXPECT_CALL(*mock_ui, SetIdentityInfo(_));
    EXPECT_CALL(*mock_ui, SetCookieInfo(_));
  }

  void SetURL(const std::string& url) {
    url_ = GURL(url);
    origin_ = url::Origin::Create(url_);
  }

  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) {
    last_chosen_object_info_.clear();
    for (auto& chosen_object_info : chosen_object_info_list)
      last_chosen_object_info_.push_back(std::move(chosen_object_info));
    last_permission_info_list_ = permission_info_list;
  }

  void ResetMockUI() {
    mock_ui_ = std::make_unique<MockPageInfoUI>();
    // Use this rather than gmock's ON_CALL.WillByDefault(Invoke(... because
    // gmock doesn't handle move-only types well.
    mock_ui_->set_permission_info_callback_ =
        base::Bind(&PageInfoTest::SetPermissionInfo, base::Unretained(this));
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
  security_state::SecurityLevel security_level() { return security_level_; }
  const security_state::VisibleSecurityState& visible_security_state() {
    return visible_security_state_;
  }
  const std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>&
  last_chosen_object_info() {
    return last_chosen_object_info_;
  }
  const PermissionInfoList& last_permission_info_list() {
    return last_permission_info_list_;
  }
  TabSpecificContentSettings* tab_specific_content_settings() {
    return TabSpecificContentSettings::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  PageInfo* page_info() {
    if (!page_info_.get()) {
      page_info_ = std::make_unique<PageInfo>(
          mock_ui(), profile(), tab_specific_content_settings(), web_contents(),
          url(), security_level(), visible_security_state());
    }
    return page_info_.get();
  }

  security_state::SecurityLevel security_level_;
  security_state::VisibleSecurityState visible_security_state_;

 private:
  std::unique_ptr<PageInfo> page_info_;
  std::unique_ptr<MockPageInfoUI> mock_ui_;
  scoped_refptr<net::X509Certificate> cert_;
  GURL url_;
  url::Origin origin_;
  std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>
      last_chosen_object_info_;
  PermissionInfoList last_permission_info_list_;
};

bool PermissionInfoListContainsPermission(const PermissionInfoList& permissions,
                                          ContentSettingsType content_type) {
  for (const auto& permission : permissions) {
    if (permission.type == content_type)
      return true;
  }
  return false;
}

}  // namespace

TEST_F(PageInfoTest, NonFactoryDefaultAndRecentlyChangedPermissionsShown) {
  page_info()->PresentSitePermissions();
  std::set<ContentSettingsType> expected_visible_permissions;

#if defined(OS_ANDROID)
  // Geolocation is always allowed to pass through to Android-specific logic to
  // check for DSE settings (so expect 1 item), but isn't actually shown later
  // on because this test isn't testing with a default search engine origin.
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
#endif
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Change some default-ask settings away from the default.
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  expected_visible_permissions.insert(ContentSettingsType::GEOLOCATION);
  page_info()->OnSitePermissionChanged(ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
  expected_visible_permissions.insert(ContentSettingsType::NOTIFICATIONS);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW);
  expected_visible_permissions.insert(ContentSettingsType::MEDIASTREAM_MIC);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  expected_visible_permissions.insert(ContentSettingsType::POPUPS);
  // Change a default-block setting to a user-preference block instead.
  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_BLOCK);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  expected_visible_permissions.insert(ContentSettingsType::JAVASCRIPT);
  // Change a default-allow setting away from the default.
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Make sure changing a setting to its default causes it to show up, since it
  // has been recently changed.
  expected_visible_permissions.insert(ContentSettingsType::MEDIASTREAM_CAMERA);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Set the Javascript setting to default should keep it shown.
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Change the default setting for Javascript away from the factory default.
  page_info()->content_settings_->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  page_info()->PresentSitePermissions();
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Change it back to ALLOW, which is its factory default, but has a source
  // from the user preference (i.e. it counts as non-factory default).
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_ALLOW);
  EXPECT_EQ(expected_visible_permissions.size(),
            last_permission_info_list().size());

  // Sanity check the correct permissions are being shown.
  for (ContentSettingsType type : expected_visible_permissions) {
    EXPECT_TRUE(PermissionInfoListContainsPermission(
        last_permission_info_list(), type));
  }
}

TEST_F(PageInfoTest, OnPermissionsChanged) {
  // Setup site permissions.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
#endif
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::NOTIFICATIONS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_MIC, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_CAMERA, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

// SetPermissionInfo() is called once initially, and then again every time
// OnSitePermissionChanged() is called.
#if !BUILDFLAG(ENABLE_PLUGINS)
  // SetPermissionInfo for plugins didn't get called.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(6);
#else
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(7);
#endif

  // Execute code under tests.
  page_info()->OnSitePermissionChanged(ContentSettingsType::POPUPS,
                                       CONTENT_SETTING_ALLOW);
#if BUILDFLAG(ENABLE_PLUGINS)
  page_info()->OnSitePermissionChanged(ContentSettingsType::PLUGINS,
                                       CONTENT_SETTING_BLOCK);
#endif
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW);

  // Verify that the site permissions were changed correctly.
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
#if BUILDFLAG(ENABLE_PLUGINS)
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
#endif
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::NOTIFICATIONS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_MIC, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), ContentSettingsType::MEDIASTREAM_CAMERA, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
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
  store->GrantDevicePermission(origin(), origin(), *device_info);

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  // Access PageInfo so that SetPermissionInfo is called once to populate
  // |last_chosen_object_info_|. It will be called again by
  // OnSiteChosenObjectDeleted.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);
  page_info();

  ASSERT_EQ(1u, last_chosen_object_info().size());
  const PageInfoUI::ChosenObjectInfo* info = last_chosen_object_info()[0].get();
  page_info()->OnSiteChosenObjectDeleted(info->ui_info,
                                         info->chooser_object->value);

  EXPECT_FALSE(store->HasDevicePermission(origin(), origin(), *device_info));
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
#if !defined(OS_ANDROID)
#define IDR_PAGEINFO_WARNING_MINOR 0
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
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content with a nonsecure form. The nonsecure form is the
      // more severe problem.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Only nonsecure form.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       false /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       false /* ran_mixed_content */, true /* displayed_mixed_content */, false,
       false /* ran_content_with_cert_errors */,
       false /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
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
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
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
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content and subresources with cert errors.
      {security_state::NONE, 0, false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */, false,
       false /* ran_mixed_content */, true /* displayed_mixed_content */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content, a nonsecure form, and subresources with cert
      // errors.
      {security_state::NONE, 0, false /* ran_mixed_content */,
       true /* displayed_mixed_content */, true,
       false /* ran_content_with_cert_errors */,
       true /* displayed_content_with_cert_errors */,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
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
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
  };

  for (const auto& test : kTestCases) {
    ResetMockUI();
    ClearPageInfo();
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
#if defined(OS_ANDROID)
    EXPECT_EQ(
        test.expected_connection_icon_id,
        PageInfoUI::GetConnectionIconID(page_info()->site_connection_status()));
#endif
  }
}

TEST_F(PageInfoTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der));
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

#if defined(OS_CHROMEOS)
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
#if defined(OS_ANDROID)
  EXPECT_EQ(IDR_PAGEINFO_WARNING_MINOR,
            PageInfoUI::GetIdentityIconID(page_info()->site_identity_status()));
#endif
}

#if !defined(OS_ANDROID)
// Tests that the site connection status is correctly set for Legacy TLS sites
// when the kLegacyTLSWarnings feature is enabled.
TEST_F(PageInfoTest, LegacyTLS) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kLegacyTLSWarnings);

  security_level_ = security_state::WARNING;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = 0;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLVersion(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;
  visible_security_state_.connection_used_legacy_tls = true;
  visible_security_state_.should_suppress_legacy_tls_warning = false;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_LEGACY_TLS,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
}

// Tests that the site connection status is not set to LEGACY_TLS when a site
// using legacy TLS is marked as a control site in the visible security state,
// when the kLegacyTLSWarnings feature is enabled.
TEST_F(PageInfoTest, LegacyTLSControlSite) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kLegacyTLSWarnings);

  security_level_ = security_state::SECURE;
  visible_security_state_.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state_.certificate = cert();
  visible_security_state_.cert_status = 0;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLVersion(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  visible_security_state_.connection_status = status;
  visible_security_state_.connection_info_initialized = true;
  visible_security_state_.connection_used_legacy_tls = true;
  visible_security_state_.should_suppress_legacy_tls_warning = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
}
#endif

#if !defined(OS_ANDROID)
TEST_F(PageInfoTest, NoInfoBar) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(0u, infobar_service()->infobar_count());
}

TEST_F(PageInfoTest, ShowInfoBar) {
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);

  EXPECT_EQ(0u, infobar_service()->infobar_count());
  page_info()->OnSitePermissionChanged(ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  bool unused;
  page_info()->OnUIClosing(&unused);
  ASSERT_EQ(1u, infobar_service()->infobar_count());

  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
}

TEST_F(PageInfoTest, NoInfoBarWhenSoundSettingChanged) {
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  page_info()->OnSitePermissionChanged(ContentSettingsType::SOUND,
                                       CONTENT_SETTING_BLOCK);
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(0u, infobar_service()->infobar_count());
}

TEST_F(PageInfoTest, ShowInfoBarWhenSoundSettingAndAnotherSettingChanged) {
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  page_info()->OnSitePermissionChanged(ContentSettingsType::JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK);
  page_info()->OnSitePermissionChanged(ContentSettingsType::SOUND,
                                       CONTENT_SETTING_BLOCK);
  bool unused;
  page_info()->OnUIClosing(&unused);
  EXPECT_EQ(1u, infobar_service()->infobar_count());

  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
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
#if defined(OS_ANDROID)
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
  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    ChromeSSLHostStateDelegate* ssl_state =
        ChromeSSLHostStateDelegateFactory::GetForProfile(profile());
    const std::string host = GURL(test.url).host();

    ssl_state->RevokeUserAllowExceptionsHard(host);
    ResetMockUI();
    SetURL(test.url);
    if (test.button_visible) {
      // In the case where the button should be visible, add an exception to
      // the profile settings for the site (since the exception is what
      // will make the button visible).
      ssl_state->AllowCert(host, *cert(), net::ERR_CERT_DATE_INVALID);
      page_info();
      if (test.button_clicked) {
        page_info()->OnRevokeSSLErrorBypassButtonPressed();
        EXPECT_FALSE(ssl_state->HasAllowException(host));
        ClearPageInfo();
        histograms.ExpectTotalCount(kGenericHistogram, 1);
        histograms.ExpectBucketCount(
            kGenericHistogram,
            PageInfo::SSLCertificateDecisionsDidRevoke::
                USER_CERT_DECISIONS_REVOKED,
            1);
      } else {  // Case where button is visible but not clicked.
        ClearPageInfo();
        EXPECT_TRUE(ssl_state->HasAllowException(host));
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
      EXPECT_FALSE(ssl_state->HasAllowException(host));
      // Button is not visible, so check histogram is empty after opening and
      // closing page info.
      histograms.ExpectTotalCount(kGenericHistogram, 0);
    }
  }
  // Test class expects PageInfo to exist during Teardown.
  page_info();
}

// Tests that metrics are recorded on a PageInfo for pages with
// various security levels.
TEST_F(PageInfoTest, SecurityLevelMetrics) {
  struct TestCase {
    const std::string url;
    const security_state::SecurityLevel security_level;
    const std::string histogram_name;
  };
  const char kGenericHistogram[] = "WebsiteSettings.Action";

  const TestCase kTestCases[] = {
      {"https://example.test", security_state::SECURE,
       "Security.PageInfo.Action.HttpsUrl.ValidNonEV"},
      {"https://example.test", security_state::EV_SECURE,
       "Security.PageInfo.Action.HttpsUrl.ValidEV"},
      {"https://example2.test", security_state::NONE,
       "Security.PageInfo.Action.HttpsUrl.Downgraded"},
      {"https://example.test", security_state::DANGEROUS,
       "Security.PageInfo.Action.HttpsUrl.Dangerous"},
      {"http://example.test", security_state::WARNING,
       "Security.PageInfo.Action.HttpUrl.Warning"},
      {"http://example.test", security_state::DANGEROUS,
       "Security.PageInfo.Action.HttpUrl.Dangerous"},
      {"http://example.test", security_state::NONE,
       "Security.PageInfo.Action.HttpUrl.Neutral"},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL(test.url);
    security_level_ = test.security_level;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);
    histograms.ExpectTotalCount(test.histogram_name, 0);

    page_info()->RecordPageInfoAction(
        PageInfo::PageInfoAction::PAGE_INFO_OPENED);

    // RecordPageInfoAction() is called during PageInfo
    // creation in addition to the explicit RecordPageInfoAction()
    // call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(kGenericHistogram,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);

    histograms.ExpectTotalCount(test.histogram_name, 2);
    histograms.ExpectBucketCount(test.histogram_name,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);
  }
}

// Tests that the duration of time the PageInfo is open is recorded for pages
// with various security levels.
TEST_F(PageInfoTest, TimeOpenMetrics) {
  struct TestCase {
    const std::string url;
    const security_state::SecurityLevel security_level;
    const std::string security_level_name;
    const PageInfo::PageInfoAction action;
  };

  const std::string kHistogramPrefix("Security.PageInfo.TimeOpen.");

  const TestCase kTestCases[] = {
      // PAGE_INFO_COUNT used as shorthand for "take no action".
      {"https://example.test", security_state::SECURE, "SECURE",
       PageInfo::PAGE_INFO_COUNT},
      {"https://example.test", security_state::EV_SECURE, "EV_SECURE",
       PageInfo::PAGE_INFO_COUNT},
      {"http://example.test", security_state::NONE, "NONE",
       PageInfo::PAGE_INFO_COUNT},
      {"https://example.test", security_state::SECURE, "SECURE",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {"https://example.test", security_state::EV_SECURE, "EV_SECURE",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {"http://example.test", security_state::NONE, "NONE",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
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
    if (test.action != PageInfo::PAGE_INFO_COUNT) {
      test_page_info->RecordPageInfoAction(test.action);
    }
    ClearPageInfo();

    histograms.ExpectTotalCount(kHistogramPrefix + test.security_level_name, 1);

    if (test.action != PageInfo::PAGE_INFO_COUNT) {
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

// Tests that metrics are recorded on a PageInfo for pages with
// various Safety Tip statuses.
TEST_F(PageInfoTest, SafetyTipMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kSafetyTipUI);
  struct TestCase {
    const security_state::SafetyTipInfo safety_tip_info;
    const std::string histogram_name;
  };
  const char kGenericHistogram[] = "WebsiteSettings.Action";

  const TestCase kTestCases[] = {
      {{security_state::SafetyTipStatus::kNone, GURL()},
       "Security.SafetyTips.PageInfo.Action.SafetyTip_None"},
      {{security_state::SafetyTipStatus::kBadReputation, GURL()},
       "Security.SafetyTips.PageInfo.Action.SafetyTip_BadReputation"},
      {{security_state::SafetyTipStatus::kLookalike, GURL()},
       "Security.SafetyTips.PageInfo.Action.SafetyTip_Lookalike"},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL("https://example.test");
    visible_security_state_.safety_tip_info = test.safety_tip_info;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);
    histograms.ExpectTotalCount(test.histogram_name, 0);

    page_info()->RecordPageInfoAction(
        PageInfo::PageInfoAction::PAGE_INFO_OPENED);

    // RecordPageInfoAction() is called during PageInfo
    // creation in addition to the explicit RecordPageInfoAction()
    // call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(kGenericHistogram,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);

    histograms.ExpectTotalCount(test.histogram_name, 2);
    histograms.ExpectBucketCount(test.histogram_name,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);
  }
}

// Tests that the duration of time the PageInfo is open is recorded for pages
// with various Safety Tip statuses.
TEST_F(PageInfoTest, SafetyTipTimeOpenMetrics) {
  struct TestCase {
    const security_state::SafetyTipStatus safety_tip_status;
    const std::string safety_tip_status_name;
    const PageInfo::PageInfoAction action;
  };

  const std::string kHistogramPrefix("Security.PageInfo.TimeOpen.");

  const TestCase kTestCases[] = {
      // PAGE_INFO_COUNT used as shorthand for "take no action".
      {security_state::SafetyTipStatus::kNone, "SafetyTip_None",
       PageInfo::PAGE_INFO_COUNT},
      {security_state::SafetyTipStatus::kLookalike, "SafetyTip_Lookalike",
       PageInfo::PAGE_INFO_COUNT},
      {security_state::SafetyTipStatus::kBadReputation,
       "SafetyTip_BadReputation", PageInfo::PAGE_INFO_COUNT},
      {security_state::SafetyTipStatus::kNone, "SafetyTip_None",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {security_state::SafetyTipStatus::kLookalike, "SafetyTip_Lookalike",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {security_state::SafetyTipStatus::kBadReputation,
       "SafetyTip_BadReputation", PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
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
    if (test.action != PageInfo::PAGE_INFO_COUNT) {
      test_page_info->RecordPageInfoAction(test.action);
    }
    ClearPageInfo();

    histograms.ExpectTotalCount(kHistogramPrefix + test.safety_tip_status_name,
                                1);

    if (test.action != PageInfo::PAGE_INFO_COUNT) {
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

// Tests that metrics are recorded on a PageInfo for pages with
// various Legacy TLS statuses.
TEST_F(PageInfoTest, LegacyTLSMetrics) {
  const struct TestCase {
    const bool connection_used_legacy_tls;
    const bool should_suppress_legacy_tls_warning;
    const std::string histogram_suffix;
  } kTestCases[] = {
      {true, false, "LegacyTLS_Triggered"},
      {true, true, "LegacyTLS_NotTriggered"},
      {false, false, "LegacyTLS_NotTriggered"},
  };

  const std::string kHistogramPrefix("Security.LegacyTLS.PageInfo.Action");
  const char kGenericHistogram[] = "WebsiteSettings.Action";

  InitializeEmptyLegacyTLSConfig();

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL("https://example.test");
    visible_security_state_.connection_used_legacy_tls =
        test.connection_used_legacy_tls;
    visible_security_state_.should_suppress_legacy_tls_warning =
        test.should_suppress_legacy_tls_warning;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);
    histograms.ExpectTotalCount(kHistogramPrefix + "." + test.histogram_suffix,
                                0);

    page_info()->RecordPageInfoAction(
        PageInfo::PageInfoAction::PAGE_INFO_OPENED);

    // RecordPageInfoAction() is called during PageInfo creation in addition to
    // the explicit RecordPageInfoAction() call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(kGenericHistogram,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);

    histograms.ExpectTotalCount(kHistogramPrefix + "." + test.histogram_suffix,
                                2);
    histograms.ExpectBucketCount(kHistogramPrefix + "." + test.histogram_suffix,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);
  }
}

// Tests that the duration of time the PageInfo is open is recorded for pages
// with various Legacy TLS statuses.
TEST_F(PageInfoTest, LegacyTLSTimeOpenMetrics) {
  const struct TestCase {
    const bool connection_used_legacy_tls;
    const bool should_suppress_legacy_tls_warning;
    const std::string legacy_tls_status_name;
    const PageInfo::PageInfoAction action;
  } kTestCases[] = {
      // PAGE_INFO_COUNT used as shorthand for "take no action".
      {true, false, "LegacyTLS_Triggered", PageInfo::PAGE_INFO_COUNT},
      {true, true, "LegacyTLS_NotTriggered", PageInfo::PAGE_INFO_COUNT},
      {false, false, "LegacyTLS_NotTriggered", PageInfo::PAGE_INFO_COUNT},
      {true, false, "LegacyTLS_Triggered",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {true, true, "LegacyTLS_NotTriggered",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
      {false, false, "LegacyTLS_NotTriggered",
       PageInfo::PAGE_INFO_SITE_SETTINGS_OPENED},
  };

  const std::string kHistogramPrefix("Security.PageInfo.TimeOpen.");

  InitializeEmptyLegacyTLSConfig();

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL("https://example.test");
    visible_security_state_.connection_used_legacy_tls =
        test.connection_used_legacy_tls;
    visible_security_state_.should_suppress_legacy_tls_warning =
        test.should_suppress_legacy_tls_warning;
    ResetMockUI();
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kHistogramPrefix + test.legacy_tls_status_name,
                                0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "Action." + test.legacy_tls_status_name, 0);
    histograms.ExpectTotalCount(
        kHistogramPrefix + "NoAction." + test.legacy_tls_status_name, 0);

    PageInfo* test_page_info = page_info();
    if (test.action != PageInfo::PAGE_INFO_COUNT) {
      test_page_info->RecordPageInfoAction(test.action);
    }
    ClearPageInfo();

    histograms.ExpectTotalCount(kHistogramPrefix + test.legacy_tls_status_name,
                                1);

    if (test.action != PageInfo::PAGE_INFO_COUNT) {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "Action." + test.legacy_tls_status_name, 1);
    } else {
      histograms.ExpectTotalCount(
          kHistogramPrefix + "NoAction." + test.legacy_tls_status_name, 1);
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
    return PermissionInfoListContainsPermission(permissions,
                                                ContentSettingsType::ADS);
  };

  // By default, the setting should not appear at all.
  SetURL("https://example.test/");
  SetDefaultUIExpectations(mock_ui());
  page_info();
  EXPECT_FALSE(showing_setting(last_permission_info_list()));

  // Reset state.
  ResetMockUI();
  ClearPageInfo();
  SetDefaultUIExpectations(mock_ui());

  // Now, simulate activation on that origin, which is encoded by the existence
  // of the website setting. The setting should then appear in page_info.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetWebsiteSettingDefaultScope(
      url(), GURL(), ContentSettingsType::ADS_DATA, std::string(),
      std::make_unique<base::DictionaryValue>());
  page_info();
  EXPECT_TRUE(showing_setting(last_permission_info_list()));
}

#if !defined(OS_ANDROID)

// Unit tests with the unified autoplay sound settings UI enabled. When enabled
// the sound settings dropdown on the page info UI will have custom wording.

class UnifiedAutoplaySoundSettingsPageInfoTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~UnifiedAutoplaySoundSettingsPageInfoTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kAutoplayDisableSettings, media::kAutoplayWhitelistSettings},
        {});
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void SetAutoplayPrefValue(bool value) {
    profile()->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, value);
  }

  void SetDefaultSoundContentSetting(ContentSetting default_setting) {
    default_setting_ = default_setting;
  }

  base::string16 GetDefaultSoundSettingString() {
    return PageInfoUI::PermissionActionToUIString(
        profile(), ContentSettingsType::SOUND, CONTENT_SETTING_DEFAULT,
        default_setting_, content_settings::SettingSource::SETTING_SOURCE_USER);
  }

  base::string16 GetSoundSettingString(ContentSetting setting) {
    return PageInfoUI::PermissionActionToUIString(
        profile(), ContentSettingsType::SOUND, setting, default_setting_,
        content_settings::SettingSource::SETTING_SOURCE_USER);
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
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BUTTON_TEXT_ALLOWED_BY_DEFAULT),
      PageInfoUI::PermissionActionToUIString(
          profile(), ContentSettingsType::ADS, CONTENT_SETTING_DEFAULT,
          CONTENT_SETTING_ALLOW,
          content_settings::SettingSource::SETTING_SOURCE_USER));
}

#endif  // !defined(OS_ANDROID)
