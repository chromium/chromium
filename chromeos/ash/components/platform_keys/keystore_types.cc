// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/platform_keys/keystore_types.h"

namespace chromeos {

KeystoreRsaParams::KeystoreRsaParams() = default;
KeystoreRsaParams::KeystoreRsaParams(const KeystoreRsaParams&) = default;
KeystoreRsaParams::KeystoreRsaParams(KeystoreRsaParams&&) noexcept = default;
KeystoreRsaParams& KeystoreRsaParams::operator=(const KeystoreRsaParams&) =
    default;
KeystoreRsaParams& KeystoreRsaParams::operator=(KeystoreRsaParams&&) noexcept =
    default;
KeystoreRsaParams::~KeystoreRsaParams() = default;

KeystoreEcdsaParams::KeystoreEcdsaParams() = default;
KeystoreEcdsaParams::KeystoreEcdsaParams(const KeystoreEcdsaParams&) = default;
KeystoreEcdsaParams::KeystoreEcdsaParams(KeystoreEcdsaParams&&) noexcept =
    default;
KeystoreEcdsaParams& KeystoreEcdsaParams::operator=(
    const KeystoreEcdsaParams&) = default;
KeystoreEcdsaParams& KeystoreEcdsaParams::operator=(
    KeystoreEcdsaParams&&) noexcept = default;
KeystoreEcdsaParams::~KeystoreEcdsaParams() = default;

RsassaPkcs115Params::RsassaPkcs115Params() = default;
RsassaPkcs115Params::RsassaPkcs115Params(const RsassaPkcs115Params&) = default;
RsassaPkcs115Params::RsassaPkcs115Params(RsassaPkcs115Params&&) noexcept =
    default;
RsassaPkcs115Params& RsassaPkcs115Params::operator=(
    const RsassaPkcs115Params&) = default;
RsassaPkcs115Params& RsassaPkcs115Params::operator=(
    RsassaPkcs115Params&&) noexcept = default;
RsassaPkcs115Params::~RsassaPkcs115Params() = default;

RsaOaepParams::RsaOaepParams() = default;
RsaOaepParams::RsaOaepParams(const RsaOaepParams&) = default;
RsaOaepParams::RsaOaepParams(RsaOaepParams&&) noexcept = default;
RsaOaepParams& RsaOaepParams::operator=(const RsaOaepParams&) = default;
RsaOaepParams& RsaOaepParams::operator=(RsaOaepParams&&) noexcept = default;
RsaOaepParams::~RsaOaepParams() = default;

GetPublicKeySuccessResult::GetPublicKeySuccessResult() = default;
GetPublicKeySuccessResult::GetPublicKeySuccessResult(
    const GetPublicKeySuccessResult&) = default;
GetPublicKeySuccessResult::GetPublicKeySuccessResult(
    GetPublicKeySuccessResult&&) noexcept = default;
GetPublicKeySuccessResult& GetPublicKeySuccessResult::operator=(
    const GetPublicKeySuccessResult&) = default;
GetPublicKeySuccessResult& GetPublicKeySuccessResult::operator=(
    GetPublicKeySuccessResult&&) noexcept = default;
GetPublicKeySuccessResult::~GetPublicKeySuccessResult() = default;

}  // namespace chromeos
