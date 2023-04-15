// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PROTO_CONVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PROTO_CONVERSIONS_H_

namespace ash::nearby::presence {

::nearby::internal::Metadata BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& account_name,
    const std::string& device_name,
    const std::string& user_name,
    const std::string& profile_url,
    const std::string& mac_address);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PROTO_CONVERSIONS_H_
