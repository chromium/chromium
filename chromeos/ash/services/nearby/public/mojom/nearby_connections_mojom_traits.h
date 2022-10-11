// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "third_party/nearby/src/internal/platform/implementation/log_message.h"

namespace mojo {

template <>
class EnumTraits<location::nearby::connections::mojom::LogSeverity,
                 location::nearby::api::LogMessage::Severity> {
 public:
  static location::nearby::connections::mojom::LogSeverity ToMojom(
      location::nearby::api::LogMessage::Severity input);
  static bool FromMojom(location::nearby::connections::mojom::LogSeverity input,
                        location::nearby::api::LogMessage::Severity* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_NEARBY_CONNECTIONS_MOJOM_TRAITS_H_
