// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_utils.h"

#include "base/uuid.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_common.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"

namespace ash::babelorca {

RequestHeader GetRequestHeaderTemplate() {
  RequestHeader request_header;
  request_header.set_app(kTachyonAppName);
  request_header.set_request_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  ClientInfo* client_info = request_header.mutable_client_info();
  client_info->set_api_version(ApiVersion::V4);
  client_info->set_platform_type(Platform::DESKTOP);
  return request_header;
}

}  // namespace ash::babelorca
