// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/oak_ct_verifier.h"

#include <optional>
#include <string>
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
    oakct_chain_der_ = {chain[0]->GetDER(), chain[1]->GetDER()};
    tca_root_der_ = {1, 2, 3, 4, 5, 6, 7, 8};
  }

  std::optional<Result> RunVerify(network::mojom::NetworkContext* context,
                                  base::span<const uint8_t> tca_root_der,
                                  const std::vector<std::string>& chain) {
    std::optional<Result> result;
    OakCtVerifier::Verify(
        context, tca_root_der, chain, kNamespace, kBaseDomain,
        base::BindLambdaForTesting([&](Result r) { result = r; }));
    return result;
  }

  std::string ExpectedHostname() const {
    return base::StrCat({*OakCtVerifier::ComputeContentId(tca_root_der_), ".",
                         kNamespace, ".", kBaseDomain});
  }

  FakeNetworkContext context_;
  std::vector<uint8_t> tca_root_der_;
  std::vector<std::string> oakct_chain_der_;
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

TEST_F(OakCtVerifierTest, BuildOakCtHostname) {
  auto hostname =
      OakCtVerifier::BuildOakCtHostname(tca_root_der_, kNamespace, kBaseDomain);
  ASSERT_TRUE(hostname.has_value());
  EXPECT_EQ(*hostname, ExpectedHostname());

  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname({}, kNamespace, kBaseDomain)
                   .has_value());
  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname(tca_root_der_, "", kBaseDomain)
                   .has_value());
  EXPECT_FALSE(OakCtVerifier::BuildOakCtHostname(tca_root_der_, kNamespace, "")
                   .has_value());
}

TEST_F(OakCtVerifierTest, VerifiedPassesHostnameAndChainToVerifier) {
  context_.set_cert_verify_result(CompliantResult());

  EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
            Result::kVerified);

  ASSERT_EQ(context_.call_count(), 1);
  EXPECT_EQ(context_.last_host_port().host(), ExpectedHostname());
  EXPECT_EQ(context_.last_host_port().port(), 443);

  // The leaf is the end-entity certificate, the rest are intermediates.
  const auto& certificate = context_.last_certificate();
  ASSERT_TRUE(certificate);
  EXPECT_EQ(
      net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
      oakct_chain_der_[0]);
  ASSERT_EQ(certificate->intermediate_buffers().size(), 1u);
  EXPECT_EQ(net::x509_util::CryptoBufferAsStringPiece(
                certificate->intermediate_buffers()[0].get()),
            oakct_chain_der_[1]);
}

TEST_F(OakCtVerifierTest, InvalidTcaRoot) {
  EXPECT_EQ(RunVerify(&context_, {}, oakct_chain_der_),
            Result::kInvalidTcaRoot);
  EXPECT_EQ(context_.call_count(), 0);
}

TEST_F(OakCtVerifierTest, InvalidOakCtCertificate) {
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, {}),
            Result::kInvalidOakCtCertificate);
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, {"not a certificate"}),
            Result::kInvalidOakCtCertificate);
  EXPECT_EQ(context_.call_count(), 0);

  // Only the leaf is parsed up front; a malformed intermediate is handed to
  // the verifier, which is expected to reject the chain.
  context_.set_net_error(net::ERR_CERT_INVALID);
  EXPECT_EQ(RunVerify(&context_, tca_root_der_,
                      {oakct_chain_der_[0], "not a certificate"}),
            Result::kCertVerifyFailed);
  EXPECT_EQ(context_.call_count(), 1);
}

TEST_F(OakCtVerifierTest, NoNetworkContext) {
  EXPECT_EQ(RunVerify(nullptr, tca_root_der_, oakct_chain_der_),
            Result::kNoNetworkContext);
}

TEST_F(OakCtVerifierTest, VerifierDisconnected) {
  context_.set_drop_callback(true);
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
            Result::kVerifierDisconnected);
  EXPECT_EQ(context_.call_count(), 1);
}

TEST_F(OakCtVerifierTest, CertVerifyFailed) {
  context_.set_net_error(net::ERR_CERT_AUTHORITY_INVALID);
  context_.set_cert_verify_result(CompliantResult());
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
            Result::kCertVerifyFailed);

  context_.set_net_error(net::ERR_CERT_COMMON_NAME_INVALID);
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
            Result::kCertVerifyFailed);
}

TEST_F(OakCtVerifierTest, NotPubliclyTrusted) {
  net::CertVerifyResult result = CompliantResult();
  result.is_issued_by_known_root = false;
  context_.set_cert_verify_result(result);
  EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
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
    EXPECT_EQ(RunVerify(&context_, tca_root_der_, oakct_chain_der_),
              Result::kCtPolicyNotMet)
        << static_cast<int>(compliance);
  }
}

}  // namespace
}  // namespace private_ai
