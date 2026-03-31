// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_mojom_traits.h"

#include "base/notreached.h"

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

  NOTREACHED();
}

nearby::api::LogMessage::Severity
EnumTraits<nearby::connections::mojom::LogSeverity,
           nearby::api::LogMessage::Severity>::
    FromMojom(nearby::connections::mojom::LogSeverity input) {
  switch (input) {
    case nearby::connections::mojom::LogSeverity::kVerbose:
      return nearby::api::LogMessage::Severity::kVerbose;
    case nearby::connections::mojom::LogSeverity::kInfo:
      return nearby::api::LogMessage::Severity::kInfo;
    case nearby::connections::mojom::LogSeverity::kWarning:
      return nearby::api::LogMessage::Severity::kWarning;
    case nearby::connections::mojom::LogSeverity::kError:
      return nearby::api::LogMessage::Severity::kError;
    case nearby::connections::mojom::LogSeverity::kFatal:
      return nearby::api::LogMessage::Severity::kFatal;
  }

  NOTREACHED();
}

}  // namespace mojo
