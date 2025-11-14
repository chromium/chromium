// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"

namespace actor_login {
MockActorLoginQualityLogger::MockActorLoginQualityLogger() = default;
MockActorLoginQualityLogger::~MockActorLoginQualityLogger() = default;

base::WeakPtr<MockActorLoginQualityLogger>
MockActorLoginQualityLogger::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace actor_login
