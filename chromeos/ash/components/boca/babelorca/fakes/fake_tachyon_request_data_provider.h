// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_

#include <string>

#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"

namespace ash::babelorca {

class FakeTachyonRequestDataProvider : public TachyonRequestDataProvider {
 public:
  FakeTachyonRequestDataProvider() = default;

  FakeTachyonRequestDataProvider(const FakeTachyonRequestDataProvider&) =
      delete;
  FakeTachyonRequestDataProvider& operator=(
      const FakeTachyonRequestDataProvider&) = delete;

  ~FakeTachyonRequestDataProvider() override = default;

  std::string session_id() override;
  std::string tachyon_token() override;
  std::string group_id() override;
  std::string sender_email() override;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TACHYON_REQUEST_DATA_PROVIDER_H_
