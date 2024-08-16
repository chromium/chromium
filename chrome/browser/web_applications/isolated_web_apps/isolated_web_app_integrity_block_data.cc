// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"

#include "base/base64.h"
#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/functional/overloaded.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/proto/web_app_isolation_data.pb.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

namespace web_app {

namespace {

template <typename KeyType>
base::expected<KeyType, std::string> PublicKeyFromBase64Data(
    const std::string& b64_data) {
  auto data = base::Base64Decode(b64_data);
  if (!data) {
    return base::unexpected("Unable to parse base64-encoded public key data.");
  }
  return KeyType::Create(*data);
}

template <typename SignatureType>
base::expected<SignatureType, std::string> SignatureFromHexData(
    const std::string& hex_data) {
  std::vector<uint8_t> data;
  if (!base::HexStringToBytes(hex_data, &data)) {
    return base::unexpected("Unable to parse hex-encoded signature data.");
  }
  return SignatureType::Create(data);
}

base::expected<web_package::SignedWebBundleSignatureInfo, std::string>
SignatureInfoFromProto(
    const proto::IsolationData::IntegrityBlockData::SignatureInfo& si_proto) {
  using SignatureInfoCase = proto::IsolationData::IntegrityBlockData::
      SignatureInfo::SignatureInfoCase;

  switch (si_proto.signature_info_case()) {
    case SignatureInfoCase::kEd25519: {
      const auto& ed25519 = si_proto.ed25519();
      ASSIGN_OR_RETURN(auto public_key,
                       PublicKeyFromBase64Data<web_package::Ed25519PublicKey>(
                           ed25519.public_key()));
      ASSIGN_OR_RETURN(auto signature,
                       SignatureFromHexData<web_package::Ed25519Signature>(
                           ed25519.signature()));
      return web_package::SignedWebBundleSignatureInfoEd25519(
          std::move(public_key), std::move(signature));
    }
    case SignatureInfoCase::kEcdsaP256Sha256: {
      const auto& ecdsa_p256_sha256 = si_proto.ecdsa_p256_sha256();
      ASSIGN_OR_RETURN(auto public_key,
                       PublicKeyFromBase64Data<web_package::EcdsaP256PublicKey>(
                           ecdsa_p256_sha256.public_key()));
      ASSIGN_OR_RETURN(
          auto signature,
          SignatureFromHexData<web_package::EcdsaP256SHA256Signature>(
              ecdsa_p256_sha256.signature()));
      return web_package::SignedWebBundleSignatureInfoEcdsaP256SHA256(
          std::move(public_key), std::move(signature));
    }
    case SignatureInfoCase::kUnknown: {
      return web_package::SignedWebBundleSignatureInfoUnknown();
    }
    case SignatureInfoCase::SIGNATURE_INFO_NOT_SET: {
      return base::unexpected("SignatureInfo is undefined.");
    }
  }
}

template <typename SignatureInfoProto>
SignatureInfoProto SignatureInfoToProto(const auto& signature_info) {
  SignatureInfoProto si_proto;
  si_proto.set_public_key(
      base::Base64Encode(signature_info.public_key().bytes()));
  si_proto.set_signature(base::HexEncode(signature_info.signature().bytes()));
  return si_proto;
}

}  // namespace

IsolatedWebAppIntegrityBlockData::IsolatedWebAppIntegrityBlockData(
    std::vector<web_package::SignedWebBundleSignatureInfo> signatures)
    : signatures_(std::move(signatures)) {}

IsolatedWebAppIntegrityBlockData::~IsolatedWebAppIntegrityBlockData() = default;

IsolatedWebAppIntegrityBlockData::IsolatedWebAppIntegrityBlockData(
    const IsolatedWebAppIntegrityBlockData&) = default;
IsolatedWebAppIntegrityBlockData& IsolatedWebAppIntegrityBlockData::operator=(
    const IsolatedWebAppIntegrityBlockData&) = default;

bool IsolatedWebAppIntegrityBlockData::operator==(
    const IsolatedWebAppIntegrityBlockData& other) const = default;

// static
IsolatedWebAppIntegrityBlockData
IsolatedWebAppIntegrityBlockData::FromIntegrityBlock(
    const web_package::SignedWebBundleIntegrityBlock& integrity_block) {
  return IsolatedWebAppIntegrityBlockData(base::ToVector(
      integrity_block.signature_stack().entries(),
      &web_package::SignedWebBundleSignatureStackEntry::signature_info));
}

// static
base::expected<IsolatedWebAppIntegrityBlockData, std::string>
IsolatedWebAppIntegrityBlockData::FromProto(
    const proto::IsolationData::IntegrityBlockData& proto) {
  std::vector<web_package::SignedWebBundleSignatureInfo> signatures;
  for (const auto& si_proto : proto.signatures()) {
    ASSIGN_OR_RETURN(auto signature_info, SignatureInfoFromProto(si_proto));
    signatures.push_back(std::move(signature_info));
  }
  return IsolatedWebAppIntegrityBlockData(std::move(signatures));
}

proto::IsolationData::IntegrityBlockData
IsolatedWebAppIntegrityBlockData::ToProto() const {
  proto::IsolationData::IntegrityBlockData proto;
  for (const auto& signature_info : signatures_) {
    proto::IsolationData::IntegrityBlockData::SignatureInfo si_proto;
    absl::visit(
        base::Overloaded{
            [&](const web_package::SignedWebBundleSignatureInfoEd25519&
                    signature_info) {
              *si_proto.mutable_ed25519() = SignatureInfoToProto<
                  proto::IsolationData::IntegrityBlockData::
                      SignatureInfoEd25519>(signature_info);
            },
            [&](const web_package::SignedWebBundleSignatureInfoEcdsaP256SHA256&
                    signature_info) {
              *si_proto.mutable_ecdsa_p256_sha256() = SignatureInfoToProto<
                  proto::IsolationData::IntegrityBlockData::
                      SignatureInfoEcdsaP256SHA256>(signature_info);
            },
            [&](const web_package::SignedWebBundleSignatureInfoUnknown&) {
              *si_proto.mutable_unknown() = proto::IsolationData::
                  IntegrityBlockData::SignatureInfoUnknown();
            }},
        signature_info);
    *proto.add_signatures() = std::move(si_proto);
  }
  return proto;
}

base::Value IsolatedWebAppIntegrityBlockData::AsDebugValue() const {
  return base::Value(base::Value::Dict().Set(
      "signatures", base::ToValueList(signatures_, [](const auto& signature) {
        return absl::visit(
            base::Overloaded{
                [](const web_package::SignedWebBundleSignatureInfoEd25519&
                       signature_info) {
                  return base::Value::Dict().Set(
                      "ed25519",
                      base::Value::Dict()
                          .Set("public_key",
                               base::Base64Encode(
                                   signature_info.public_key().bytes()))
                          .Set("signature",
                               base::HexEncode(
                                   signature_info.signature().bytes())));
                },
                [](const web_package::
                       SignedWebBundleSignatureInfoEcdsaP256SHA256&
                           signature_info) {
                  return base::Value::Dict().Set(
                      "ecdsa_p256_sha256",
                      base::Value::Dict()
                          .Set("public_key",
                               base::Base64Encode(
                                   signature_info.public_key().bytes()))
                          .Set("signature",
                               base::HexEncode(
                                   signature_info.signature().bytes())));
                },
                [](const web_package::SignedWebBundleSignatureInfoUnknown&
                       public_key) {
                  return base::Value::Dict().Set("unknown",
                                                 base::Value::Dict());
                }},
            signature);
      })));
}

bool IsolatedWebAppIntegrityBlockData::HasPublicKey(
    base::span<const uint8_t> public_key) const {
  return base::ranges::any_of(signatures(), [&](const auto& signature_info) {
    return absl::visit(
        base::Overloaded{
            [&](const auto& signature_info) {
              return base::ranges::equal(signature_info.public_key().bytes(),
                                         public_key);
            },
            [](const web_package::SignedWebBundleSignatureInfoUnknown&) {
              return false;
            }},
        signature_info);
  });
}

}  // namespace web_app
