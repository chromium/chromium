// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/system_permission_delegate_factory.h"

#include "base/notreached.h"

std::unique_ptr<EmbeddedPermissionPrompt::SystemPermissionDelegate>
SystemPermissionDelegateFactory::CreateSystemPermissionDelegate(
    ContentSettingsType type) {
  // TODO(crbug.com/336977114, crbug.com/336977253, crbug.com/336977018):
  // Implement handling for each specific system permission type (e.g., camera,
  // microphone, location).
  switch (type) {
    default:
      NOTREACHED_NORETURN();
  }
}
