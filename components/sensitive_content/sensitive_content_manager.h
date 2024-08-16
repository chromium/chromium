// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
#define COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_

#include "base/memory/raw_ref.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class SensitiveContentClient;

// Contains platform-independent logic which tracks whether sensitive form
// fields are present or not. It is owned by the embedder-specific
// implementation of `SensitiveContentClient`.
class SensitiveContentManager final {
 public:
  SensitiveContentManager(content::WebContents* web_contents,
                          SensitiveContentClient* client);

  SensitiveContentManager(const SensitiveContentManager&) = delete;
  SensitiveContentManager& operator=(const SensitiveContentManager&) = delete;
  ~SensitiveContentManager();

 private:
  const raw_ref<SensitiveContentClient> client_;
};

}  // namespace sensitive_content

#endif  // COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
