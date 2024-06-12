// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_auth_util_fuzzer_shared.h"

#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {
namespace fuzz {

using ::openscreen::cast::proto::CastMessage;

void SetupAuthenticateChallengeReplyInput(
    const std::vector<std::string>& certs,
    CastAuthUtilInputs::AuthenticateChallengeReplyInput* input) {
  // If we have a DeviceAuthMessage, use it to override the cast_message()
  // payload with a more interesting value.
  if (input->has_auth_message()) {
    // Optimization: if the payload_binary() field is going to be
    // overwritten, insist that it has to be empty initially.  This cuts
    // down on how much time is spent generating identical arguments for
    // AuthenticateChallengeReply() from different values of |input|.
    if (input->cast_message().has_payload_binary()) {
      return;
    }

    if (!input->auth_message().has_response()) {
      // Optimization.
      if (input->nonce_ok() || input->response_certs_ok() ||
          input->tbs_crls_size() || input->crl_certs_ok() ||
          input->crl_signatures_ok()) {
        return;
      }
    } else {
      openscreen::cast::proto::AuthResponse& response =
          *input->mutable_auth_message()->mutable_response();

      // Maybe force the nonce to be the correct value.
      if (input->nonce_ok()) {
        // Optimization.
        if (response.has_sender_nonce()) {
          return;
        }

        response.set_sender_nonce(input->nonce());
      }

      // Maybe force the response certs to be valid.
      if (input->response_certs_ok()) {
        // Optimization.
        if (!response.client_auth_certificate().empty() ||
            response.intermediate_certificate_size() > 0) {
          return;
        }

        response.set_client_auth_certificate(certs.front());
        response.clear_intermediate_certificate();
        for (std::size_t i = 1; i < certs.size(); i++) {
          response.add_intermediate_certificate(certs.at(i));
        }
      }

      // Maybe replace the crl() field in the response with valid data.
      if (input->tbs_crls_size() == 0) {
        // Optimization.
        if (input->crl_certs_ok() || input->crl_signatures_ok()) {
          return;
        }
      } else {
        // Optimization.
        if (response.has_crl()) {
          return;
        }

        openscreen::cast::proto::CrlBundle crl_bundle;
        for (const auto& tbs_crl : input->tbs_crls()) {
          openscreen::cast::proto::Crl& crl = *crl_bundle.add_crls();
          if (input->crl_certs_ok()) {
            crl.set_signer_cert(certs.at(0));
          }
          if (input->crl_signatures_ok()) {
            crl.set_signature("");
          }
          tbs_crl.SerializeToString(crl.mutable_tbs_crl());
        }
        crl_bundle.SerializeToString(response.mutable_crl());
      }
    }

    input->mutable_cast_message()->set_payload_type(CastMessage::BINARY);
    input->auth_message().SerializeToString(
        input->mutable_cast_message()->mutable_payload_binary());
  }
}

}  // namespace fuzz
}  // namespace cast_channel
