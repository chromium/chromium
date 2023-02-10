// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_channel_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

namespace cast_channel {

void RecordCertificateStatus(CastCertificateStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate", status);
}

void RecordNonceStatus(CastNonceStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Nonce", status);
}

void RecordSignatureStatus(CastSignatureStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Signature", status);
}

void RecordLaunchSessionChannelFlags(CastChannelFlags flags) {
  if (!flags) {
    UMA_HISTOGRAM_ENUMERATION(kLaunchSessionChannelFlagsHistogram,
                              CastChannelFlag::kFlagsNone);
    return;
  }
  for (int flag_idx = 0; flag_idx < kNumCastChannelFlags; flag_idx++) {
    if (flags & static_cast<uint16_t>(1 << flag_idx)) {
      UMA_HISTOGRAM_ENUMERATION(kLaunchSessionChannelFlagsHistogram,
                                static_cast<CastChannelFlag>(1 << flag_idx));
    }
  }
}

}  // namespace cast_channel
