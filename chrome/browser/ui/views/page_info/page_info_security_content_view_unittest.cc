// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/features.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace {

// A certificate whose subject has both an organization name and a country, so
// that the subtitle of the QWAC rows is populated.
scoped_refptr<net::X509Certificate> CertWithOrganizationAndCountry() {
  return net::X509Certificate::CreateFromBytes(thawte_der);
}

// A certificate whose subject has an organization but no country. The subtitle
// requires both, so this one produces no subtitle.
scoped_refptr<net::X509Certificate> CertWithoutCountry() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 "duplicate_cn_1.pem");
}

scoped_refptr<net::X509Certificate> SimpleCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
}

std::u16string ExpectedSubjectText(const net::X509Certificate& cert) {
  return l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
      base::UTF8ToUTF16(cert.subject().organization_names[0]),
      base::UTF8ToUTF16(cert.subject().country_name));
}

std::u16string QwacIconLabel() {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_QWAC_ICON_A11Y_LABEL);
}

std::u16string QwacTitle() {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_QWAC_STATUS_TITLE);
}

}  // namespace

class PageInfoSecurityContentViewTestBase : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    // `PageInfoSecurityContentView` only dereferences its presenter from button
    // callbacks, which these tests do not invoke. Passing null is only safe
    // because this view is not a standalone page, which would initialize the
    // presenter's UI state.
    view_ = std::make_unique<PageInfoSecurityContentView>(
        /*presenter=*/nullptr, /*is_standalone_page=*/false);
  }

  void TearDown() override {
    view_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::View* GetViewById(PageInfoViewFactory::PageInfoViewID id) {
    return view_->GetViewByID(id);
  }

  std::unique_ptr<PageInfoSecurityContentView> view_;
};

class PageInfoSecurityContentViewTest
    : public PageInfoSecurityContentViewTestBase {
 public:
  PageInfoSecurityContentViewTest() {
    feature_list_.InitAndDisableFeature(net::features::kVerifyQWACs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PageInfoSecurityContentViewQwacTest
    : public PageInfoSecurityContentViewTestBase {
 public:
  PageInfoSecurityContentViewQwacTest() {
    feature_list_.InitAndEnableFeature(net::features::kVerifyQWACs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The 1-QWAC row is not a button and contains no focusable controls, so it must
// be exposed as an accessibility-only focus target naming its whole contents.
// Otherwise a screen reader following focus skips the row and the user never
// learns that the site has EU qualified status.
TEST_F(PageInfoSecurityContentViewQwacTest, OneQwacRowIsAccessible) {
  scoped_refptr<net::X509Certificate> cert = CertWithOrganizationAndCountry();
  ASSERT_TRUE(cert);

  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = cert;
  view_->SetIdentityInfo(identity);

  views::View* row =
      GetViewById(PageInfoViewFactory::VIEW_ID_PAGE_INFO_ONE_QWAC_INFORMATION);
  ASSERT_TRUE(row);

  EXPECT_EQ(row->GetFocusBehavior(),
            views::View::FocusBehavior::ACCESSIBLE_ONLY);
  EXPECT_EQ(row->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kGroup);
  EXPECT_EQ(row->GetViewAccessibility().GetCachedName(),
            QwacIconLabel() + u"\n" + QwacTitle() + u"\n" +
                ExpectedSubjectText(*cert));
}

// Certificates without both an organization and a country have no subtitle, and
// the accessible name must not contain a dangling separator.
TEST_F(PageInfoSecurityContentViewQwacTest, OneQwacRowWithoutSubtitle) {
  scoped_refptr<net::X509Certificate> cert = CertWithoutCountry();
  ASSERT_TRUE(cert);
  ASSERT_TRUE(cert->subject().country_name.empty());

  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_1QWAC_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = cert;
  view_->SetIdentityInfo(identity);

  views::View* row =
      GetViewById(PageInfoViewFactory::VIEW_ID_PAGE_INFO_ONE_QWAC_INFORMATION);
  ASSERT_TRUE(row);

  EXPECT_EQ(row->GetViewAccessibility().GetCachedName(),
            QwacIconLabel() + u"\n" + QwacTitle());
}

// Sites without a 1-QWAC must not get the row at all.
TEST_F(PageInfoSecurityContentViewQwacTest, NoOneQwacRowForRegularCert) {
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = SimpleCert();
  ASSERT_TRUE(identity.certificate);
  view_->SetIdentityInfo(identity);

  EXPECT_FALSE(
      GetViewById(PageInfoViewFactory::VIEW_ID_PAGE_INFO_ONE_QWAC_INFORMATION));
}

// The 2-QWAC row is a button, whose icons are not reachable on their own, so
// their descriptions must be part of the button's accessible name.
TEST_F(PageInfoSecurityContentViewQwacTest, TwoQwacButtonIsAccessible) {
  scoped_refptr<net::X509Certificate> two_qwac =
      CertWithOrganizationAndCountry();
  ASSERT_TRUE(two_qwac);

  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = SimpleCert();
  ASSERT_TRUE(identity.certificate);
  identity.two_qwac = two_qwac;
  view_->SetIdentityInfo(identity);

  views::View* button = GetViewById(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_TWO_QWAC_CERTIFICATE_VIEWER);
  ASSERT_TRUE(button);

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            QwacIconLabel() + u"\n" + QwacTitle() + u"\n" +
                ExpectedSubjectText(*two_qwac) + u"\n" +
                l10n_util::GetStringUTF16(IDS_PAGE_INFO_OPEN_QWAC_A11Y_LABEL));
}

// The certificate row's launch icon is labelled regardless of whether QWAC
// verification is enabled, since the icon is shown to every user.
TEST_F(PageInfoSecurityContentViewTest, CertificateButtonActionIconIsLabelled) {
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = SimpleCert();
  ASSERT_TRUE(identity.certificate);
  view_->SetIdentityInfo(identity);

  views::View* button = GetViewById(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER);
  ASSERT_TRUE(button);

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERTIFICATE_IS_VALID) +
                u"\n" +
                l10n_util::GetStringUTF16(
                    IDS_PAGE_INFO_OPEN_CERTIFICATE_DETAILS_A11Y_LABEL));
}

TEST_F(PageInfoSecurityContentViewQwacTest,
       CertificateButtonActionIconIsLabelledWithQwacsEnabled) {
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity.certificate = SimpleCert();
  ASSERT_TRUE(identity.certificate);
  view_->SetIdentityInfo(identity);

  views::View* button = GetViewById(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER);
  ASSERT_TRUE(button);

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERTIFICATE_DETAILS) +
                u"\n" +
                l10n_util::GetStringUTF16(
                    IDS_PAGE_INFO_OPEN_CERTIFICATE_DETAILS_A11Y_LABEL));
}
