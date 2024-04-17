// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_USER_DATA_JAPANESE_LEGACY_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_IME_USER_DATA_JAPANESE_LEGACY_CONFIG_H_

#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"

namespace ash::ime {

mojom::JapaneseLegacyConfigPtr MakeMojomJapaneseLegacyConfig(
    chromeos_input::FetchJapaneseLegacyConfigResponse proto_response);

}  // namespace ash::ime

#endif  // CHROMEOS_ASH_SERVICES_IME_USER_DATA_JAPANESE_LEGACY_CONFIG_H_
