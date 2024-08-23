// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_service.h"

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

ReadAnythingService::ReadAnythingService(Profile* profile) : profile_(profile) {
  // TODO(https://crbug.com/355485153): This class is currently a stub.
  extensions::ExtensionSystem::Get(profile_)->extension_service();
}
