// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

SharedWebContentsWithAppLock::SharedWebContentsWithAppLock(
    base::flat_set<AppId> app_ids)
    : Lock(std::move(app_ids), Lock::Type::kAppAndWebContents) {}
SharedWebContentsWithAppLock::~SharedWebContentsWithAppLock() = default;

}  // namespace web_app
