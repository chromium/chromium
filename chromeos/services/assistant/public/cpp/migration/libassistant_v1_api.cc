// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"

#include "base/check.h"
#include "base/check_op.h"

namespace chromeos {
namespace assistant {

// static
LibassistantV1Api* LibassistantV1Api::instance_ = nullptr;

LibassistantV1Api::LibassistantV1Api(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal)
    : assistant_manager_(assistant_manager),
      assistant_manager_internal_(assistant_manager_internal) {
  DCHECK(!instance_);
  instance_ = this;
}

LibassistantV1Api::~LibassistantV1Api() {
  instance_ = nullptr;
}

}  // namespace assistant
}  // namespace chromeos
