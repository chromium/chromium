// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_

#include "chromeos/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
class EnumTraits<chromeos::secure_channel::mojom::ConnectionMedium,
                 chromeos::secure_channel::ConnectionMedium> {
 public:
  static chromeos::secure_channel::mojom::ConnectionMedium ToMojom(
      chromeos::secure_channel::ConnectionMedium input);
  static bool FromMojom(chromeos::secure_channel::mojom::ConnectionMedium input,
                        chromeos::secure_channel::ConnectionMedium* out);
};

template <>
class EnumTraits<chromeos::secure_channel::mojom::ConnectionPriority,
                 chromeos::secure_channel::ConnectionPriority> {
 public:
  static chromeos::secure_channel::mojom::ConnectionPriority ToMojom(
      chromeos::secure_channel::ConnectionPriority input);
  static bool FromMojom(
      chromeos::secure_channel::mojom::ConnectionPriority input,
      chromeos::secure_channel::ConnectionPriority* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_MOJOM_SECURE_CHANNEL_MOJOM_TRAITS_H_
