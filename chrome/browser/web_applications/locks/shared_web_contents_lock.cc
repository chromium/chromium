// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

SharedWebContentsLock::SharedWebContentsLock()
    : Lock({}, Lock::Type::kBackgroundWebContents) {}
SharedWebContentsLock::~SharedWebContentsLock() = default;

}  // namespace web_app
