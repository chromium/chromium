// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_mojom_traits.h"

namespace mojo {

location::nearby::connections::mojom::LogSeverity
EnumTraits<location::nearby::connections::mojom::LogSeverity,
           location::nearby::api::LogMessage::Severity>::
    ToMojom(location::nearby::api::LogMessage::Severity input) {
  switch (input) {
    case location::nearby::api::LogMessage::Severity::kVerbose:
      return location::nearby::connections::mojom::LogSeverity::kVerbose;
    case location::nearby::api::LogMessage::Severity::kInfo:
      return location::nearby::connections::mojom::LogSeverity::kInfo;
    case location::nearby::api::LogMessage::Severity::kWarning:
      return location::nearby::connections::mojom::LogSeverity::kWarning;
    case location::nearby::api::LogMessage::Severity::kError:
      return location::nearby::connections::mojom::LogSeverity::kError;
    case location::nearby::api::LogMessage::Severity::kFatal:
      return location::nearby::connections::mojom::LogSeverity::kFatal;
  }

  NOTREACHED();
  return location::nearby::connections::mojom::LogSeverity::kInfo;
}

bool EnumTraits<location::nearby::connections::mojom::LogSeverity,
                location::nearby::api::LogMessage::Severity>::
    FromMojom(location::nearby::connections::mojom::LogSeverity input,
              location::nearby::api::LogMessage::Severity* out) {
  switch (input) {
    case location::nearby::connections::mojom::LogSeverity::kVerbose:
      *out = location::nearby::api::LogMessage::Severity::kVerbose;
      return true;
    case location::nearby::connections::mojom::LogSeverity::kInfo:
      *out = location::nearby::api::LogMessage::Severity::kInfo;
      return true;
    case location::nearby::connections::mojom::LogSeverity::kWarning:
      *out = location::nearby::api::LogMessage::Severity::kWarning;
      return true;
    case location::nearby::connections::mojom::LogSeverity::kError:
      *out = location::nearby::api::LogMessage::Severity::kError;
      return true;
    case location::nearby::connections::mojom::LogSeverity::kFatal:
      *out = location::nearby::api::LogMessage::Severity::kFatal;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
