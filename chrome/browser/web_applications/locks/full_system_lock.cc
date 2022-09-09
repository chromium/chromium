// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/full_system_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

FullSystemLock::FullSystemLock() : Lock({}, Lock::Type::kFullSystem) {}
FullSystemLock::~FullSystemLock() = default;

}  // namespace web_app
