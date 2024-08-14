// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_CLIENT_H_
#define COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_CLIENT_H_

namespace sensitive_content {

// Used for dependency injection from the embedder (Chrome on Android or
// WebView).
class SensitiveContentClient {
 public:
  virtual ~SensitiveContentClient() = default;

  virtual void SetContentSensitivity(bool content_is_sensitive) = 0;
};

}  // namespace sensitive_content

#endif  // COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_CLIENT_H_
