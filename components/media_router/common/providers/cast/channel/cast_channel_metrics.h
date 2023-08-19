// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_

#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

namespace cast_channel {

constexpr char kLaunchSessionChannelFlagsHistogram[] =
    "Cast.Channel.LaunchSession.Flags";

// Must match with histogram enum CastCertificateStatus.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CastCertificateStatus {
  kOk = 0,
  kInvalidCRL,
  kVerificationFailed,
  kRevoked,
  kMissingCRL,
  kParseFailed,
  kDateInvalid,
  kRestrictionsFailed,
  kMissingCerts,
  kUnexpectedFailed,
  kInvalidFallbackCRL,
  kCertificateRevokedByFallbackCRL,
  kCertificateAcceptedByFallbackCRL,
  kMaxValue = kCertificateAcceptedByFallbackCRL,
};

// Must match with histogram enum CastNonce.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CastNonceStatus {
  kMatch = 0,
  kMismatch,
  kMissing,
  kMaxValue = kMissing,
};

// Must match with the histogram enum CastSignature.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CastSignatureStatus {
  kOk = 0,
  kEmpty,
  kVerifyFailed,
  kAlgorithmUnsupported,
  kMaxValue = kAlgorithmUnsupported,
};

// Record certificate verification histogram events.
void RecordCertificateStatus(CastCertificateStatus status);

// Record nonce verification histogram events.
void RecordNonceStatus(CastNonceStatus status);

// Record signature verification histogram events.
void RecordSignatureStatus(CastSignatureStatus status);

// Records the flags set on a Cast channel when a Cast LAUNCH message is sent.
void RecordLaunchSessionChannelFlags(CastChannelFlags flags);

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_
