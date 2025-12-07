// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/with_shared_web_contents_resources.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WithSharedWebContentsResources::WithSharedWebContentsResources() = default;
WithSharedWebContentsResources::~WithSharedWebContentsResources() = default;

content::WebContents& WithSharedWebContentsResources::shared_web_contents()
    const {
  CHECK(lock_manager_);
  CHECK(shared_web_contents_);
  return *shared_web_contents_;
}

void WithSharedWebContentsResources::GrantWithSharedWebContentsResources(
    WebAppLockManager& lock_manager,
    content::WebContents& shared_web_contents) {
  lock_manager_ = lock_manager.GetWeakPtr();
  shared_web_contents_ = shared_web_contents.GetWeakPtr();
}

}  // namespace web_app
