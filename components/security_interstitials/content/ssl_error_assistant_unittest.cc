// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_assistant.h"

#include <memory>

#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "content/public/test/test_renderer_host.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};

const uint32_t kLargeVersionId = 0xFFFFFFu;

// These certificates are self signed certificates with relevant issuer common
// names generated using the following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes

// Common name: "Misconfig Software"
// Organization name: "Test Company"
const char kMisconfigSoftwareCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC5DCCAk2gAwIBAgIJAPYPMpr0AIDBMA0GCSqGSIb3DQEBBQUAMFYxCzAJBgNV\n"
    "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUwEwYDVQQKEwxUZXN0IENvbXBh\n"
    "bnkxGzAZBgNVBAMTEk1pc2NvbmZpZyBTb2Z0d2FyZTAeFw0xNzEwMTcxOTQyMzFa\n"
    "Fw0xODEwMTcxOTQyMzFaMFYxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0\n"
    "YXRlMRUwEwYDVQQKEwxUZXN0IENvbXBhbnkxGzAZBgNVBAMTEk1pc2NvbmZpZyBT\n"
    "b2Z0d2FyZTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAyZWnLvuD19iG5PSi\n"
    "8dSVhLeuZDBtwcWBOMzga3hx7HDMd+395gstRLc1VhpMePmxUdyEpStHDiYjNF/k\n"
    "GRsIXfXWpO82L7r+Fm6eym4BOw2sjX1aounljETYasREvXhEB/8WaLJfMcstUwsT\n"
    "PoXgUWYkIBi/76EiWHXvYEiXV2kCAwEAAaOBuTCBtjAdBgNVHQ4EFgQUtakrb0wU\n"
    "gZVXyus1vlj6T5aDEnYwgYYGA1UdIwR/MH2AFLWpK29MFIGVV8rrNb5Y+k+WgxJ2\n"
    "oVqkWDBWMQswCQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEVMBMGA1UE\n"
    "ChMMVGVzdCBDb21wYW55MRswGQYDVQQDExJNaXNjb25maWcgU29mdHdhcmWCCQD2\n"
    "DzKa9ACAwTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4GBAFAFlPO3HEWQ\n"
    "0XdRbeIQPVva72VFyF+ESFC6ky7GLDoaSAwRlE1i5qWfxnLbEA0b7CWjyO1tC8Uw\n"
    "OMB5U9qmQouAqf5medr2pECSDimb7qBCz3kKjgZWt1Xv8w0PsW6lFVPmMsO4Zv7F\n"
    "Podf1biETWgaYoT6PrUTtWG3jeSU2r9M\n"
    "-----END CERTIFICATE-----";

// Common name: "ijklmn opqrs"
// Organization name: "abc defgh co"
const char kMisconfigSoftwareRegexCheckCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC0jCCAjugAwIBAgIJAOyyORCXGxvDMA0GCSqGSIb3DQEBBQUAMFAxCzAJBgNV\n"
    "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUwEwYDVQQKEwxhYmMgZGVmZ2gg\n"
    "Y28xFTATBgNVBAMTDGlqa2xtbiBvcHFyczAeFw0xNzEwMTcyMjM4MzJaFw0xODEw\n"
    "MTcyMjM4MzJaMFAxCzAJBgNVBAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRUw\n"
    "EwYDVQQKEwxhYmMgZGVmZ2ggY28xFTATBgNVBAMTDGlqa2xtbiBvcHFyczCBnzAN\n"
    "BgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAsnuBPW2k4+eFazC8lq7rLRNjpZ5yqEwX\n"
    "LBE8fxbvjXSSZAaJz/iTn+Zg/UMJz9IpulbcA/xf36JuhFYv7aClFrtg5DHaqrPf\n"
    "kt7g9AM3hEIjGsdHtyAqFp/+CpySGzVpTLyT1NtHkqtkiD6HCSpWqL+m/6ibpUhy\n"
    "oy9y/ZV1vVUCAwEAAaOBszCBsDAdBgNVHQ4EFgQUBk+vtSjNipTcWh3NIbtsjVN0\n"
    "uJswgYAGA1UdIwR5MHeAFAZPr7UozYqU3FodzSG7bI1TdLiboVSkUjBQMQswCQYD\n"
    "VQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEVMBMGA1UEChMMYWJjIGRlZmdo\n"
    "IGNvMRUwEwYDVQQDEwxpamtsbW4gb3BxcnOCCQDssjkQlxsbwzAMBgNVHRMEBTAD\n"
    "AQH/MA0GCSqGSIb3DQEBBQUAA4GBACv8KnNmaOqHD8QsmvaD2Yvc7dAFkCgsdQb/\n"
    "Tkyw0sJN8ZH+bummkgGZLw4gzdmhVg8kGIbiDvCYgOVaIg+2H3PtkdIrW2KhyXrN\n"
    "2qIa9nBvuv8LC1TAdB65DDheLh0PuTGcIwfJ7kcKi+Eo8fPbYYdyHGRw+rVWXVPz\n"
    "SgZO4ZYq\n"
    "-----END CERTIFICATE-----";

}  // namespace

class SSLErrorAssistantTest : public content::RenderViewHostTestHarness {
 public:
  SSLErrorAssistantTest(const SSLErrorAssistantTest&) = delete;
  SSLErrorAssistantTest& operator=(const SSLErrorAssistantTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    error_assistant_ = std::make_unique<SSLErrorAssistant>();

    ssl_info_.cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "subjectAltName_www_example_com.pem");
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));
  }

  void TearDown() override {
    error_assistant_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void TestMITMSoftwareMatchFromString(const std::string& cert,
                                       const std::string& match_result) {
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            base::as_bytes(base::make_span(cert)),
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(certs.empty());
    EXPECT_EQ(match_result,
              error_assistant()->MatchKnownMITMSoftware(certs[0]));
  }

 protected:
  SSLErrorAssistantTest() {
    embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>();
  }

  ~SSLErrorAssistantTest() override = default;

  SSLErrorAssistant* error_assistant() const { return error_assistant_.get(); }

  net::EmbeddedTestServer* embedded_test_server() const {
    return embedded_test_server_.get();
  }

  const net::SSLInfo& ssl_info() const { return ssl_info_; }

  const std::string& issuer_common_name() const {
    return ssl_info_.cert->issuer().common_name;
  }

  const std::string& issuer_organization_name() const {
    DCHECK(!ssl_info_.cert->issuer().organization_names.empty());
    return ssl_info_.cert->issuer().organization_names.front();
  }

 private:
  net::SSLInfo ssl_info_;

  std::unique_ptr<SSLErrorAssistant> error_assistant_;
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;
};

// Test to see if IsKnownCaptivePortalCertificate() returns the correct value.
TEST_F(SSLErrorAssistantTest, CaptivePortalCertificateList) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  // Test without the known captive portal certificate in config_proto.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  config_proto->add_captive_portal_cert()->set_sha256_hash("sha256/boxfish");
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      "sha256/treecreeper");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->IsKnownCaptivePortalCertificate(ssl_info()));

  error_assistant()->ResetForTesting();

  // Test with the known captive portal certificate in config_proto.
  config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  config_proto->add_captive_portal_cert()->set_sha256_hash("sha256/boxfish");
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      ssl_info().public_key_hashes[0].ToString());
  config_proto->add_captive_portal_cert()->set_sha256_hash(
      "sha256/treecreeper");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  EXPECT_TRUE(error_assistant()->IsKnownCaptivePortalCertificate(ssl_info()));
}

// Test to see if the MitM Software gets matched correctly.
TEST_F(SSLErrorAssistantTest, MitMSoftwareMatching) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Tests for a basic and more complex regex match.
  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Basic Check");
  filter->set_issuer_common_name_regex("Misconfig Software");
  filter->set_issuer_organization_regex("Test Company");

  filter = config_proto->add_mitm_software();
  filter->set_name("Regex Check");
  filter->set_issuer_common_name_regex("ij[a-z]+n opqrs");
  filter->set_issuer_organization_regex("abc de[a-z0-9]gh [a-z]+");
  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  TestMITMSoftwareMatchFromString(kMisconfigSoftwareCert, "Basic Check");
  TestMITMSoftwareMatchFromString(kMisconfigSoftwareRegexCheckCert,
                                  "Regex Check");

  error_assistant()->ResetForTesting();

  // Tests for no matches.
  config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  filter = config_proto->add_mitm_software();
  filter->set_name("Incorrect common name");
  filter->set_issuer_common_name_regex("Misconfig Sotware");
  filter->set_issuer_organization_regex("Test Company");

  filter = config_proto->add_mitm_software();
  filter->set_name("Incorrect company name");
  filter->set_issuer_common_name_regex("Misconfig Software");
  filter->set_issuer_organization_regex("Tst Company");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  TestMITMSoftwareMatchFromString(kMisconfigSoftwareCert, "");
}

// Test to see if the dynamic interstitial is matched with more complex regex
// fields.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListMatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  filter->set_mitm_software_name("UwS");
  filter->set_issuer_common_name_regex("whippoorwill");

  // Add a matching dynamic interstitial.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_mitm_software_name("UwS");
  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(issuer_organization_name());

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListComplexRegexMatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  filter->set_mitm_software_name("UwS");
  filter->set_issuer_common_name_regex("whippoorwill");

  // Add a dynamic interstitial that will match.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_mitm_software_name("UwS");
  filter->set_issuer_common_name_regex("[0-9]+.0.[0-9]+.1");
  filter->set_issuer_organization_regex("T[a-z]+t CA");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched when the certificate
// error is set to UNKNOWN_CERT_ERROR.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListMatchUnknownCertError) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  // Add a dynamic interstitial that will match.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  EXPECT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched if an empty issuer
// common name regex is set.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListNoCommonName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  // Add a matching dynamic interstitial with an empty issuer common name
  // regex.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex(std::string());
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched if no issuer
// organization name regex is set.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListNoOrganizationRegex) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  // Add a matching dynamic interstitial with an empty issuer organization
  // name regex.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(std::string());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched if no certificate hash is
// provided.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListNoCertHashes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                    INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);
  filter->add_sha256_hash("sha256/nightjar");
  filter->add_sha256_hash("sha256/frogmouth");
  filter->add_sha256_hash("sha256/poorwill");

  // Add a dynamic interstitial that will match.
  filter = config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test to see if the dynamic interstitial is matched if no certificate hash,
// cert error or regexes is provided.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListMatchBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that will mismatch.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      error_assistant()->MatchDynamicInterstitial(ssl_info());
  ASSERT_TRUE(dynamic_interstitial);
  EXPECT_EQ(chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL,
            dynamic_interstitial->interstitial_type);
}

// Test for a dynamic interstitial mismatch in the cert error.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListCertErrorMismatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->MatchDynamicInterstitial(ssl_info()));
}

// Test for a dynamic interstitial mismatch in the certificate hashes.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListHashesMismatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

  filter->add_sha256_hash("sha256/yellowlegs");
  filter->add_sha256_hash("sha256/killdeer");

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->MatchDynamicInterstitial(ssl_info()));
}

// Test for a dynamic interstitial with an issuer common name regex mismatch.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListCommonNameMismatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex("beeeater");
  filter->set_issuer_organization_regex(issuer_organization_name());
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->MatchDynamicInterstitial(ssl_info()));
}

// Test for a dynamic interstitial with an issuer organization regex mismatch.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListOrganizationMismatch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_issuer_common_name_regex(issuer_common_name());
  filter->set_issuer_organization_regex("beeeater");
  filter->set_mitm_software_name("UwS");

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));
  EXPECT_FALSE(error_assistant()->MatchDynamicInterstitial(ssl_info()));
}

// Tests that a dynamic interstitial is not triggered if the error thrown
// is overridable and the show_only_for_nonoverridable_errors flag is set.
TEST_F(SSLErrorAssistantTest, DynamicInterstitialListOverridable) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  // Add a dynamic interstitial that would match, except that the
  // show_only_for_nonoverridable_errors flag is set.
  chrome_browser_ssl::DynamicInterstitial* filter =
      config_proto->add_dynamic_interstitial();
  filter->set_interstitial_type(
      chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
  filter->set_cert_error(
      chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);
  filter->add_sha256_hash("sha256/nuthatch");
  filter->add_sha256_hash(ssl_info().public_key_hashes[0].ToString());
  filter->add_sha256_hash("sha256/treecreeper");

  filter->set_mitm_software_name("UwS");
  filter->set_issuer_common_name_regex("[0-9]+.0.[0-9]+.1");
  filter->set_issuer_organization_regex("T[a-z]+t CA");

  filter->set_show_only_for_nonoverridable_errors(true);

  error_assistant()->SetErrorAssistantProto(std::move(config_proto));

  EXPECT_FALSE(error_assistant()->MatchDynamicInterstitial(ssl_info(), true));
}
