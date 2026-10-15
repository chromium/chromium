// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/oak_ct_verifier.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/base32/base32.h"
#include "crypto/hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace private_ai {

namespace {

// Port is required by VerifyCert but irrelevant for name matching.
constexpr uint16_t kOakCtPort = 443;

void OnVerifyCertResult(OakCtVerifier::VerifyCallback callback,
                        int net_error,
                        const net::CertVerifyResult& result,
                        bool /*pkp_bypassed*/) {
  if (net_error != net::OK) {
    DVLOG(1) << "OakCT certificate verification failed: "
             << net::ErrorToString(net_error);
    std::move(callback).Run(OakCtVerifier::Result::kCertVerifyFailed);
    return;
  }
  if (!result.is_issued_by_known_root) {
    std::move(callback).Run(OakCtVerifier::Result::kNotPubliclyTrusted);
    return;
  }
  if (result.policy_compliance !=
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS) {
    DVLOG(1) << "OakCT certificate does not comply with CT policy: "
             << static_cast<int>(result.policy_compliance);
    std::move(callback).Run(OakCtVerifier::Result::kCtPolicyNotMet);
    return;
  }
  std::move(callback).Run(OakCtVerifier::Result::kVerified);
}

}  // namespace

// static
std::optional<std::string> OakCtVerifier::ComputeContentId(
    base::span<const uint8_t> content) {
  if (content.empty()) {
    return std::nullopt;
  }

  std::array<uint8_t, crypto::hash::kSha256Size> hash =
      crypto::hash::Sha256(content);

  std::string base32_encoded =
      base32::Base32Encode(hash, base32::Base32EncodePolicy::OMIT_PADDING);
  std::string content_id = base::ToLowerASCII(base32_encoded);

  if (content_id.length() != kContentIdLength) {
    return std::nullopt;
  }

  return content_id;
}

// static
std::optional<std::string> OakCtVerifier::BuildOakCtHostname(
    base::span<const uint8_t> tca_root_pem,
    std::string_view namespace_str,
    std::string_view base_domain) {
  std::optional<std::string> content_id = ComputeContentId(tca_root_pem);
  if (!content_id.has_value() || namespace_str.empty() || base_domain.empty()) {
    return std::nullopt;
  }
  return base::StrCat({*content_id, ".", namespace_str, ".", base_domain});
}

// static
void OakCtVerifier::Verify(network::mojom::NetworkContext* network_context,
                           base::span<const uint8_t> tca_root_pem,
                           base::span<const uint8_t> oakct_chain_pem,
                           std::string_view namespace_str,
                           std::string_view base_domain,
                           VerifyCallback callback) {
  std::optional<std::string> hostname =
      BuildOakCtHostname(tca_root_pem, namespace_str, base_domain);
  if (!hostname.has_value()) {
    std::move(callback).Run(Result::kInvalidTcaRoot);
    return;
  }

  // The published file holds the leaf first, followed by any intermediates.
  // Parsing stops at the first block that is not a certificate; the
  // certificate verifier is the authority on whether the chain is complete.
  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          oakct_chain_pem, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  scoped_refptr<net::X509Certificate> certificate;
  if (!certs.empty()) {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    for (size_t i = 1; i < certs.size(); ++i) {
      intermediates.push_back(bssl::UpRef(certs[i]->cert_buffer()));
    }
    certificate = net::X509Certificate::CreateFromBuffer(
        bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  }
  if (!certificate) {
    std::move(callback).Run(Result::kInvalidOakCtCertificate);
    return;
  }

  if (!network_context) {
    std::move(callback).Run(Result::kNoNetworkContext);
    return;
  }

  // SCTs are expected to be embedded in the OakCT leaf certificate, so no
  // separate SCT list or OCSP response is supplied. If the NetworkContext
  // connection is dropped before replying, the wrapped callback fails
  // closed with kVerifierDisconnected.
  network_context->VerifyCert(
      std::move(certificate), net::HostPortPair(*hostname, kOakCtPort),
      /*ocsp_response=*/std::string(), /*sct_list=*/std::string(),
      base::BindOnce(&OnVerifyCertResult,
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(callback), Result::kVerifierDisconnected)));
}

}  // namespace private_ai
