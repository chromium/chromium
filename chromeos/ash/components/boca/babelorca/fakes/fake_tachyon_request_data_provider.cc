// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"

#include <string>

namespace ash::babelorca {

std::string FakeTachyonRequestDataProvider::client_uuid() {
  return "client-uuid";
}

std::string FakeTachyonRequestDataProvider::tachyon_token() {
  return "tachyon_token";
}

std::string FakeTachyonRequestDataProvider::group_id() {
  return "group-id";
}

}  // namespace ash::babelorca
