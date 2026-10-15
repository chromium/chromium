// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_OAK_CT_VERIFIER_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_OAK_CT_VERIFIER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {

// Verifier for Oak Certificate Transparency (go/oak-ct).
//
// A Trusted Certificate Authority (TCA) root certificate is anchored in the
// public Certificate Transparency ecosystem through an "OakCT certificate": a
// publicly trusted, CT-logged TLS certificate issued for the DNS name
//
//   <content_id>.<namespace>.<base_domain>
//
// where <content_id> is the lowercase, unpadded base32 encoding of the SHA-256
// hash of the TCA root exactly as published, i.e. of the bytes of its PEM
// file (not of the DER it decodes to). Verifying that a TCA root has been
// published therefore consists of deriving the expected hostname from the TCA
// root and verifying the OakCT certificate chain for that hostname with
// Chrome's regular certificate verifier (public roots plus CT policy). The
// verification itself runs in the network service via
// NetworkContext::VerifyCert so that the browser process never parses
// untrusted certificates.
class OakCtVerifier {
 public:
  // Expected length of the ContentID (SHA-256 base32 without padding).
  static constexpr size_t kContentIdLength = 52;

  // Outcome of Verify(). Recorded in UMA
  // (PrivateAi.Certs.OakCtVerificationResult),
  // so entries must not be reordered or renumbered.
  // LINT.IfChange(OakCtVerificationResult)
  enum class Result {
    kVerified = 0,
    // The ContentID could not be computed from the TCA root.
    kInvalidTcaRoot = 1,
    // The OakCT file contained no PEM-encoded X.509 certificate.
    kInvalidOakCtCertificate = 2,
    // No NetworkContext was available to perform the verification.
    kNoNetworkContext = 3,
    // The NetworkContext went away before answering.
    kVerifierDisconnected = 4,
    // The certificate verifier rejected the chain for the OakCT hostname
    // (e.g. untrusted issuer, name mismatch, expired).
    kCertVerifyFailed = 5,
    // The chain verified, but not up to a publicly trusted root.
    kNotPubliclyTrusted = 6,
    // The chain verified, but did not satisfy the CT policy via SCTs.
    kCtPolicyNotMet = 7,
    kMaxValue = kCtPolicyNotMet,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivateAiOakCtVerificationResult)

  using VerifyCallback = base::OnceCallback<void(Result)>;

  // Computes the ContentID of the given published content: its SHA-256 hash
  // in lowercase base32 without padding (exactly 52 characters).
  static std::optional<std::string> ComputeContentId(
      base::span<const uint8_t> content);

  // Returns the OakCT hostname expected for `tca_root_pem`, i.e.
  // <content_id>.<namespace_str>.<base_domain>, or std::nullopt if the
  // ContentID cannot be computed.
  static std::optional<std::string> BuildOakCtHostname(
      base::span<const uint8_t> tca_root_pem,
      std::string_view namespace_str,
      std::string_view base_domain);

  // Verifies that the OakCT certificate chain in `oakct_chain_pem` (PEM
  // bytes, leaf first, followed by any intermediates, exactly as published)
  // is a publicly trusted, CT-compliant certificate for the OakCT hostname
  // derived from `tca_root_pem`, `namespace_str` and `base_domain`.
  //
  // `network_context` may be null, in which case the result is
  // kNoNetworkContext. `callback` is run exactly once, possibly synchronously
  // for failures detected before contacting the network service, and is
  // guaranteed to run even if the NetworkContext connection is dropped.
  static void Verify(network::mojom::NetworkContext* network_context,
                     base::span<const uint8_t> tca_root_pem,
                     base::span<const uint8_t> oakct_chain_pem,
                     std::string_view namespace_str,
                     std::string_view base_domain,
                     VerifyCallback callback);
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_OAK_CT_VERIFIER_H_
