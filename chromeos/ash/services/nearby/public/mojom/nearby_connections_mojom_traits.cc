// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_mojom_traits.h"

namespace mojo {

nearby::connections::mojom::LogSeverity
EnumTraits<nearby::connections::mojom::LogSeverity,
           nearby::api::LogMessage::Severity>::
    ToMojom(nearby::api::LogMessage::Severity input) {
  switch (input) {
    case nearby::api::LogMessage::Severity::kVerbose:
      return nearby::connections::mojom::LogSeverity::kVerbose;
    case nearby::api::LogMessage::Severity::kInfo:
      return nearby::connections::mojom::LogSeverity::kInfo;
    case nearby::api::LogMessage::Severity::kWarning:
      return nearby::connections::mojom::LogSeverity::kWarning;
    case nearby::api::LogMessage::Severity::kError:
      return nearby::connections::mojom::LogSeverity::kError;
    case nearby::api::LogMessage::Severity::kFatal:
      return nearby::connections::mojom::LogSeverity::kFatal;
  }

  NOTREACHED_IN_MIGRATION();
  return nearby::connections::mojom::LogSeverity::kInfo;
}

bool EnumTraits<nearby::connections::mojom::LogSeverity,
                nearby::api::LogMessage::Severity>::
    FromMojom(nearby::connections::mojom::LogSeverity input,
              nearby::api::LogMessage::Severity* out) {
  switch (input) {
    case nearby::connections::mojom::LogSeverity::kVerbose:
      *out = nearby::api::LogMessage::Severity::kVerbose;
      return true;
    case nearby::connections::mojom::LogSeverity::kInfo:
      *out = nearby::api::LogMessage::Severity::kInfo;
      return true;
    case nearby::connections::mojom::LogSeverity::kWarning:
      *out = nearby::api::LogMessage::Severity::kWarning;
      return true;
    case nearby::connections::mojom::LogSeverity::kError:
      *out = nearby::api::LogMessage::Severity::kError;
      return true;
    case nearby::connections::mojom::LogSeverity::kFatal:
      *out = nearby::api::LogMessage::Severity::kFatal;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
