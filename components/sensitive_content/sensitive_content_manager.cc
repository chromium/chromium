// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/sensitive_content_manager.h"

#include "base/check_deref.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "content/public/browser/web_contents.h"

namespace sensitive_content {

SensitiveContentManager::SensitiveContentManager(
    content::WebContents* web_contents,
    SensitiveContentClient* client)
    : client_(CHECK_DEREF(client)) {}

SensitiveContentManager::~SensitiveContentManager() = default;

}  // namespace sensitive_content
