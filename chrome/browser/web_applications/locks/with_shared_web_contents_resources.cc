// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/with_shared_web_contents_resources.h"

#include "base/memory/weak_ptr.h"

namespace web_app {

WithSharedWebContentsResources::WithSharedWebContentsResources(
    base::WeakPtr<WebAppLockManager> lock_manager,
    content::WebContents& shared_web_contents)
    : lock_manager_(std::move(lock_manager)),
      shared_web_contents_(shared_web_contents) {}
WithSharedWebContentsResources::~WithSharedWebContentsResources() = default;

content::WebContents& WithSharedWebContentsResources::shared_web_contents()
    const {
  CHECK(lock_manager_);
  return *shared_web_contents_;
}

}  // namespace web_app
