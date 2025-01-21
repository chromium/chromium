// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_webui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/server_certificate_database/server_certificate_database.h"
#include "components/server_certificate_database/server_certificate_database_test_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/ip_address.h"
#include "net/cert/x509_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test framework for
// chrome/test/data/webui/certificate_viewer_dialog_browsertest.js.
class CertificateViewerUITest : public WebUIMochaBrowserTest {
 protected:
  CertificateViewerUITest() {
    // Set a loadable URL so that loading the initial tab doesn't fail.
    // Certificate viewer tests run in a constrained dialog containing the
    // certificate viewer WebContents that is on top of this initial tab.
    set_test_loader_host(chrome::kChromeUIDefaultHost);
  }

  virtual bssl::UniquePtr<CRYPTO_BUFFER> GetCerts() {
    return net::x509_util::CreateCryptoBuffer(base::span(google_der));
  }

  virtual std::optional<
      chrome_browser_server_certificate_database::CertificateMetadata>
  GetCertMetadata() {
    return std::nullopt;
  }

  virtual CertMetadataModificationsCallback GetModificationsCallback() {
    return base::NullCallback();
  }

  void RunTestCase(const std::string& testCase) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert = GetCerts();
    ASSERT_TRUE(cert);
    std::optional<
        chrome_browser_server_certificate_database::CertificateMetadata>
        cert_metadata = GetCertMetadata();

    content::WebContents* dialog_contents =
        ShowCertificateViewer(std::move(cert), std::move(cert_metadata));
    ASSERT_TRUE(dialog_contents);

    ASSERT_TRUE(RunTestOnWebContents(dialog_contents,
        "certificate_viewer_dialog/certificate_viewer_dialog_test.js",
        base::StringPrintf("runMochaTest('CertificateViewer', '%s');",
                           testCase.c_str()),
        /*skip_test_loader=*/ true));
  }

 private:
  content::WebContents* ShowCertificateViewer(
      bssl::UniquePtr<CRYPTO_BUFFER> cert,
      std::optional<
          chrome_browser_server_certificate_database::CertificateMetadata>
          cert_metadata) {
    if (!browser() || !browser()->window()) {
      return nullptr;
    }

    CertificateViewerDialog* dialog;
    if (cert_metadata) {
      dialog = CertificateViewerDialog::ShowConstrainedWithMetadata(
          std::move(cert), std::move(*cert_metadata),
          GetModificationsCallback(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          browser()->window()->GetNativeWindow());
    } else {
      dialog = CertificateViewerDialog::ShowConstrained(
          std::move(cert), browser()->tab_strip_model()->GetActiveWebContents(),
          browser()->window()->GetNativeWindow());
    }

    content::WebContents* webui_webcontents =
        dialog->delegate_->GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(webui_webcontents));
    webui_webcontents->GetPrimaryMainFrame()->SetWebUIProperty(
        "expectedUrl", chrome::kChromeUICertificateViewerURL);
    return webui_webcontents;
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, DialogURL) {
  RunTestCase("DialogURL");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, CommonName) {
  RunTestCase("CommonName");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, Details) {
  RunTestCase("Details");
}

class CertificateViewerUIInvalidCertTest : public CertificateViewerUITest {
 protected:
  bssl::UniquePtr<CRYPTO_BUFFER> GetCerts() override {
    const uint8_t kInvalid[] = {42, 42, 42, 42, 42};
    return net::x509_util::CreateCryptoBuffer(base::span(kInvalid));
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUIInvalidCertTest, InvalidCert) {
  RunTestCase("InvalidCert");
}

class CertificateViewerUIWithMetadataCertTest : public CertificateViewerUITest {
 protected:
  std::optional<chrome_browser_server_certificate_database::CertificateMetadata>
  GetCertMetadata() override {
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata;
    cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    cert_metadata.mutable_constraints()->add_dns_names("example.com");
    cert_metadata.mutable_constraints()->add_dns_names("domainname.com");
    chrome_browser_server_certificate_database::CIDR* cidr =
        cert_metadata.mutable_constraints()->add_cidrs();
    cidr->set_ip(std::string(
        base::as_string_view(net::IPAddress::IPv4Localhost().bytes())));
    cidr->set_prefix_length(24);
    return cert_metadata;
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertTest,
                       CheckMetadataNotEditable) {
  RunTestCase("CheckMetadataNotEditable");
}

class CertificateViewerUIWithMetadataCertNoConstraintsTest
    : public CertificateViewerUITest {
 protected:
  std::optional<chrome_browser_server_certificate_database::CertificateMetadata>
  GetCertMetadata() override {
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata;
    cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    return cert_metadata;
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertNoConstraintsTest,
                       CheckMetadataNotEditableNoConstraints) {
  RunTestCase("CheckMetadataNotEditableNoConstraints");
}

class CertificateViewerUIWithMetadataCertEditNoConstraintsTest
    : public CertificateViewerUITest {
 public:
  void ModifyCallback(net::ServerCertificateDatabase::CertInformation,
                      base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(true);
  }

 protected:
  std::optional<chrome_browser_server_certificate_database::CertificateMetadata>
  GetCertMetadata() override {
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata;
    cert_metadata.mutable_trust()->set_trust_type(
        chrome_browser_server_certificate_database::CertificateTrust::
            CERTIFICATE_TRUST_TYPE_TRUSTED);
    return cert_metadata;
  }

  CertMetadataModificationsCallback GetModificationsCallback() override {
    return base::BindRepeating(
        &CertificateViewerUIWithMetadataCertEditNoConstraintsTest::
            ModifyCallback,
        base::Unretained(this));
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditNoConstraintsTest,
                       CheckMetadataEditableNoConstraints) {
  RunTestCase("CheckMetadataEditableNoConstraints");
}

class CertificateViewerUIWithMetadataCertEditTest
    : public CertificateViewerUIWithMetadataCertTest {
 public:
  void ModifyCallback(net::ServerCertificateDatabase::CertInformation,
                      base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(true);
  }

 protected:
  CertMetadataModificationsCallback GetModificationsCallback() override {
    return base::BindRepeating(
        &CertificateViewerUIWithMetadataCertEditTest::ModifyCallback,
        base::Unretained(this));
  }
};

// TODO(crbug.com/40928765): add C++ unit test of handler since Mocha tests are
// meant to be unit tests of the TS/HTML/CSS code.
IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       CheckMetadataEditable) {
  RunTestCase("CheckMetadataEditable");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       EditTrustState) {
  RunTestCase("EditTrustState");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       EditTrustStateError) {
  RunTestCase("EditTrustStateError");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       AddConstraintDNS) {
  RunTestCase("AddConstraintDNS");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       AddConstraintCIDR) {
  RunTestCase("AddConstraintCIDR");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       AddConstraintError) {
  RunTestCase("AddConstraintError");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       DeleteConstraintDNS) {
  RunTestCase("DeleteConstraintDNS");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       DeleteConstraintCIDR) {
  RunTestCase("DeleteConstraintCIDR");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUIWithMetadataCertEditTest,
                       DeleteConstraintError) {
  RunTestCase("DeleteConstraintError");
}
