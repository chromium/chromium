// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_

#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<ash::secure_channel::mojom::ConnectionMedium,
                 ash::secure_channel::ConnectionMedium> {
 public:
  static ash::secure_channel::mojom::ConnectionMedium ToMojom(
      ash::secure_channel::ConnectionMedium input);
  static bool FromMojom(ash::secure_channel::mojom::ConnectionMedium input,
                        ash::secure_channel::ConnectionMedium* out);
};

template <>
class EnumTraits<ash::secure_channel::mojom::ConnectionPriority,
                 ash::secure_channel::ConnectionPriority> {
 public:
  static ash::secure_channel::mojom::ConnectionPriority ToMojom(
      ash::secure_channel::ConnectionPriority input);
  static bool FromMojom(ash::secure_channel::mojom::ConnectionPriority input,
                        ash::secure_channel::ConnectionPriority* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_
