// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/oak_ct_verifier.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

using Result = OakCtVerifier::Result;

constexpr char kNamespace[] = "tca";
constexpr char kBaseDomain[] = "oakct.transparentrelease.goog";

// A NetworkContext whose VerifyCert records its arguments and answers with a
// canned result (or drops the callback).
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void set_net_error(int net_error) { net_error_ = net_error; }
  void set_cert_verify_result(const net::CertVerifyResult& result) {
    result_ = result;
  }
  void set_drop_callback(bool drop) { drop_callback_ = drop; }

  int call_count() const { return call_count_; }
  const scoped_refptr<net::X509Certificate>& last_certificate() const {
    return last_certificate_;
  }
  const net::HostPortPair& last_host_port() const { return last_host_port_; }

  // network::TestNetworkContext:
  void VerifyCert(const scoped_refptr<net::X509Certificate>& certificate,
                  const net::HostPortPair& host_port,
                  const std::string& ocsp_result,
                  const std::string& sct_list,
                  VerifyCertCallback callback) override {
    ++call_count_;
    last_certificate_ = certificate;
    last_host_port_ = host_port;
    if (drop_callback_) {
      return;
    }
    std::move(callback).Run(net_error_, result_, /*pkp_bypassed=*/false);
  }

 private:
  int net_error_ = net::OK;
  net::CertVerifyResult result_;
  bool drop_callback_ = false;
  int call_count_ = 0;
  scoped_refptr<net::X509Certificate> last_certificate_;
  net::HostPortPair last_host_port_;
};

net::CertVerifyResult CompliantResult() {
  net::CertVerifyResult result;
  result.is_issued_by_known_root = true;
  result.policy_compliance =
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  return result;
}

class OakCtVerifierTest : public testing::Test {
 protected:
  OakCtVerifierTest() {
    auto chain = net::CertBuilder::CreateSimpleChain2();
    oakct_leaf_der_ = chain[0]->GetDER();
    oakct_intermediate_der_ = chain[1]->GetDER();
    oakct_leaf_pem_ = chain[0]->GetPEM();
    oakct_chain_pem_ = oakct_leaf_pem_ + chain[1]->GetPEM();
    tca_root_pem_ = {1, 2, 3, 4, 5, 6, 7, 8};
  }

  std::optional<Result> RunVerify(network::mojom::NetworkContext* context,
                                  base::span<const uint8_t> tca_root_pem,
                                  base::span<const uint8_t> chain_pem) {
    std::optional<Result> result;
    OakCtVerifier::Verify(
        context, tca_root_pem, chain_pem, kNamespace, kBaseDomain,
        base::BindLambdaForTesting([&](Result r) { result = r; }));
    return result;
  }

  std::optional<Result> RunVerify(network::mojom::NetworkContext* context,
                                  base::span<const uint8_t> tca_root_pem,
                                  std::string_view chain_pem) {
    return RunVerify(context, tca_root_pem, base::as_byte_span(chain_pem));
  }

  std::string ExpectedHostname() const {
    return base::StrCat({*OakCtVerifier::ComputeContentId(tca_root_pem_), ".",
                         kNamespace, ".", kBaseDomain});
  }

  FakeNetworkContext context_;
  std::vector<uint8_t> tca_root_pem_;
  std::string oakct_leaf_der_;
  std::string oakct_intermediate_der_;
  std::string oakct_leaf_pem_;
  std::string oakct_chain_pem_;
};

TEST_F(OakCtVerifierTest, ComputeContentIdLength) {
  std::vector<uint8_t> fake_cert = {1, 2, 3, 4, 5};
  auto content_id = OakCtVerifier::ComputeContentId(fake_cert);
  ASSERT_TRUE(content_id.has_value());
  EXPECT_EQ(content_id->length(), OakCtVerifier::kContentIdLength);

  // Content ID should be lowercase base32 and contain no padding '='.
  for (char c : *content_id) {
    EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= '2' && c <= '7'));
  }
}

TEST_F(OakCtVerifierTest, ComputeContentIdEmpty) {
  EXPECT_FALSE(OakCtVerifier::ComputeContentId({}).has_value());
}

TEST_F(OakCtVerifierTest, ComputeContentIdKnownValue) {
  // SHA-256("abc") =
  // ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad,
  // base32 (RFC 4648, lowercase, unpadded):
  const std::string input = "abc";
  auto content_id = OakCtVerifier::ComputeContentId(base::as_byte_span(input));
  ASSERT_TRUE(content_id.has_value());
  EXPECT_EQ(*content_id,
            "xj4bnp4pahh6uqkbidpf3lrceoyagyndsylxvhfucd7wd4qacwwq");
}

TEST_F(OakCtVerifierTest, ComputeContentIdMatchesPublishedOakCtCertificate) {
  // The production TCA root as published by OakCT at
  // https://www.gstatic.com/oakct.transparentrelease.goog/tca/
  // (prod_tca_root_certificate.pem/prod_tca_root_certificate.pem).
  constexpr char kProdTcaRootPem[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIF1DCCA7ygAwIBAgIUQBmw0nG3CBL5yKFhwnv66RK5Mo0wDQYJKoZIhvcNAQEL\n"
      "BQAwNTELMAkGA1UEBhMCVVMxEzARBgNVBAoMCkdvb2dsZSBMTEMxETAPBgNVBAMM\n"
      "CFRDQSBSb290MB4XDTI2MDcyNjE2NTM1MFoXDTI3MDIwMjEzMDAwM1owNTELMAkG\n"
      "A1UEBhMCVVMxEzARBgNVBAoMCkdvb2dsZSBMTEMxETAPBgNVBAMMCFRDQSBSb290\n"
      "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAnu22adVGaGFIb4buA9EJ\n"
      "9xd2kc866hdCNfaT28Myh6RGjZ33YHV1ARSUfeaobB12vW3I7gYswZTahiFEFxKS\n"
      "pEEq+l5aWkkfGXVEklNRimGhulrHjYnjfrf2WJcDkwk9wfm3wewHxtYz5Fdpn1A1\n"
      "W3eC16zLtXXwrsDvazBlo7pnLgt5hDVc4kXYhzhaT3OyqrjP4AjE1bm5F3uIMqed\n"
      "eKWJ1Qf6D8XgWsAfecHPy1Cd/Cy4agvv03u4mO89PTvVuI/u6HLt6s/fw/Ox5XTo\n"
      "E7UH50W7dV7d3+Eo1U98AYh6ahNAVVlkVz+EDU4xa3pD4DcZKqMgUe8lVIfoYYKp\n"
      "3ReIfjWT+WbrWoqqJjTQam6M8QVjLbfaOJwQ53+pH0IpV3KDpbt4g/tgSaVkZZd4\n"
      "STEvDbVFBvKM0D7YgpcYVwMAD0sJ+b8rA7AR8U6LB6l94nXMJsQQ2g8P2ZWonaFL\n"
      "km66vbjVb6uDiGiJrV1X4c7xtUzK5PdMq/FlrUvakaF21kNyyt7ealtmxy0b3Wel\n"
      "CEvpFC4fEPVpgj5clEZgFCK4sinxUzkqSdbnlBcD1C8adv1hHI44YO3opol0t6tp\n"
      "JiI3Bjp+LC/G0xwrYYc6ubNIT4L1HFy0aS2evFRCjMGnS3Sie+Ba6xZKo124syrj\n"
      "x76QzlCLztsSqaBp+pUutb8CAwEAAaOB2zCB2DAPBgNVHRMBAf8EBTADAQH/MA4G\n"
      "A1UdDwEB/wQEAwICBDAdBgNVHQ4EFgQUpr84WOI8naDhJx9RKNwleRlNIGMwgZUG\n"
      "A1UdEQSBjTCBioaBh3NwaWZmZTovL3RjYS5wY2l0Lmdvb2cvb3BlcmF0b3IvcGNp\n"
      "dC5nb29nLzIyNzU5MzYxNTUwNi9wdWJsaXNoZXIvZ29vZ2xlLmNvbS9wY2l0LXJl\n"
      "bGVhc2UtYm90L3dvcmtsb2FkL3RyYW5zcGFyZW50LWNlcnRpZmljYXRlLWF1dGhv\n"
      "cml0eTANBgkqhkiG9w0BAQsFAAOCAgEAYdo4xTimPG9hOpNt5ADlgn/Fdqfim9R3\n"
      "950SJciup9wlD0qOgkdeLlbSVdG3002s+12hzVD9CUDW8zNa3z27PYLPB/o8H25+\n"
      "WRnBBR3v1fdmS00babdVqZcXy4sPuRhJCoE0NbKsd2Sk/hpfJVqwhOzZBMCBMxKD\n"
      "t8Bx17vaEUfRvj2+/3pfPN0PCuRJDHFVMvqw2Crzlj/N3luI9FdQgFldJpIeYXL+\n"
      "Mza+JaZ1zkHr8xwbn8el6Z83iHhtJu2oe8vUK7xMTwKfJObPohRhj2t9e8DkohGb\n"
      "u2QidJy2U2jcb0nDf56fKJ6gw5mF0sO+7a/n67uJc6hC5uvX3TFsZ+QSxTmZ7irJ\n"
      "IwbuFSRHvjUhCG4tVW/p/SqmryWbrvp/Yy5ldcNrDKg6nfkojdnDFWdrOuxb4sxK\n"
      "up7nEyqkeHRDFD2COlTep/d5UpIAGGT8O+LnBSfFamRdQAo9ZBo3+8Oa7WJb8qXm\n"
      "B792oWP39CIjuONHOv7Kbnt2UOjos3qy7Pn75x3KyhPPBvkTWICV4tMzwXm2xXmH\n"
      "PMWygyIeYWeQFcivLMURraTV05opARBWo+cY1WfNfZAHmzGQEwnXo0LkPiRhYbDi\n"
      "PSqbWjssN3j6EGvqCX0vL9y/U7FCHW8Fwcb1zuN3C/96Ne1HbTMUbV2RjHGER2aA\n"
      "ZLOMQy4xVQQ=\n"
      "-----END CERTIFICATE-----\n";
  // The DNS SAN of the OakCT certificate published alongside it
  // (prod_tca_root_certificate.pem.pem) is <content_id>.tca.oakct.
  // transparentrelease.goog with the ContentID below: the hash of the PEM
  // file as published, not of the DER it decodes to.
  EXPECT_EQ(OakCtVerifier::ComputeContentId(
                base::as_byte_span(std::string_view(kProdTcaRootPem))),
            "ks72zdcjf26huk3dr4rgm3cj6hlfrxfe7lnlegc7cka6ieuapp3q");
}

TEST_F(OakCtVerifierTest, BuildOakCtHostname) {
  auto hostname =
      OakCtVerifier::BuildOakCtHostname(tca_root_pem_, kNamespace, kBaseDomain);
  ASSERT_TRUE(hostname.has_value());
  EXPECT_EQ(*hostname, ExpectedHostname());

  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname({}, kNamespace, kBaseDomain)
                   .has_value());
  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname(tca_root_pem_, "", kBaseDomain)
                   .has_value());
  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname(tca_root_pem_, kNamespace, "")
                   .has_value());
}

TEST_F(OakCtVerifierTest, VerifiedPassesHostnameAndChainToVerifier) {
  context_.set_cert_verify_result(CompliantResult());

  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
            Result::kVerified);

  ASSERT_EQ(context_.call_count(), 1);
  EXPECT_EQ(context_.last_host_port().host(), ExpectedHostname());
  EXPECT_EQ(context_.last_host_port().port(), 443);

  // The leaf is the end-entity certificate, the rest are intermediates.
  const auto& certificate = context_.last_certificate();
  ASSERT_TRUE(certificate);
  EXPECT_EQ(
      net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
      oakct_leaf_der_);
  ASSERT_EQ(certificate->intermediate_buffers().size(), 1u);
  EXPECT_EQ(net::x509_util::CryptoBufferAsStringPiece(
                certificate->intermediate_buffers()[0].get()),
            oakct_intermediate_der_);
}

TEST_F(OakCtVerifierTest, InvalidTcaRoot) {
  EXPECT_EQ(RunVerify(&context_, {}, oakct_chain_pem_),
            Result::kInvalidTcaRoot);
  EXPECT_EQ(context_.call_count(), 0);
}

TEST_F(OakCtVerifierTest, InvalidOakCtCertificate) {
  // A PEM block whose contents ("not a cert") are not a certificate.
  constexpr char kNotACertificatePem[] =
      "-----BEGIN CERTIFICATE-----\nbm90IGEgY2VydA==\n"
      "-----END CERTIFICATE-----\n";

  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, ""),
            Result::kInvalidOakCtCertificate);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, "not a certificate"),
            Result::kInvalidOakCtCertificate);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, kNotACertificatePem),
            Result::kInvalidOakCtCertificate);
  EXPECT_EQ(context_.call_count(), 0);

  // Parsing stops at the first block that is not a certificate, so a
  // malformed intermediate leaves the verifier with the leaf alone, which it
  // is expected to reject.
  context_.set_net_error(net::ERR_CERT_INVALID);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_,
                      oakct_leaf_pem_ + kNotACertificatePem),
            Result::kCertVerifyFailed);
  ASSERT_EQ(context_.call_count(), 1);
  EXPECT_TRUE(context_.last_certificate()->intermediate_buffers().empty());
}

TEST_F(OakCtVerifierTest, NoNetworkContext) {
  EXPECT_EQ(RunVerify(nullptr, tca_root_pem_, oakct_chain_pem_),
            Result::kNoNetworkContext);
}

TEST_F(OakCtVerifierTest, VerifierDisconnected) {
  context_.set_drop_callback(true);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
            Result::kVerifierDisconnected);
  EXPECT_EQ(context_.call_count(), 1);
}

TEST_F(OakCtVerifierTest, CertVerifyFailed) {
  context_.set_net_error(net::ERR_CERT_AUTHORITY_INVALID);
  context_.set_cert_verify_result(CompliantResult());
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
            Result::kCertVerifyFailed);

  context_.set_net_error(net::ERR_CERT_COMMON_NAME_INVALID);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
            Result::kCertVerifyFailed);
}

TEST_F(OakCtVerifierTest, NotPubliclyTrusted) {
  net::CertVerifyResult result = CompliantResult();
  result.is_issued_by_known_root = false;
  context_.set_cert_verify_result(result);
  EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
            Result::kNotPubliclyTrusted);
}

TEST_F(OakCtVerifierTest, CtPolicyNotMet) {
  for (auto compliance :
       {net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
        net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
        net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
        net::ct::CTPolicyCompliance::
            CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE}) {
    net::CertVerifyResult result = CompliantResult();
    result.policy_compliance = compliance;
    context_.set_cert_verify_result(result);
    EXPECT_EQ(RunVerify(&context_, tca_root_pem_, oakct_chain_pem_),
              Result::kCtPolicyNotMet)
        << static_cast<int>(compliance);
  }
}

}  // namespace
}  // namespace private_ai
