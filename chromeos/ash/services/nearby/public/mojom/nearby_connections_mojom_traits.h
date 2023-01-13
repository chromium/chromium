// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "third_party/nearby/src/internal/platform/implementation/log_message.h"

namespace mojo {

template <>
class EnumTraits<nearby::connections::mojom::LogSeverity,
                 nearby::api::LogMessage::Severity> {
 public:
  static nearby::connections::mojom::LogSeverity ToMojom(
      nearby::api::LogMessage::Severity input);
  static bool FromMojom(nearby::connections::mojom::LogSeverity input,
                        nearby::api::LogMessage::Severity* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_
