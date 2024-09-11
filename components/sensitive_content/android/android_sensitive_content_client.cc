// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/android/android_sensitive_content_client.h"

#include "base/notreached.h"
#include "content/public/browser/web_contents.h"

namespace sensitive_content {

AndroidSensitiveContentClient::AndroidSensitiveContentClient(
    content::WebContents* web_contents,
    std::string histogram_prefix)
    : content::WebContentsUserData<AndroidSensitiveContentClient>(
          *web_contents),
      manager_(web_contents, this),
      histogram_prefix_(std::move(histogram_prefix)) {}

AndroidSensitiveContentClient::~AndroidSensitiveContentClient() = default;

void AndroidSensitiveContentClient::SetContentSensitivity(
    bool content_is_sensitive) {}

std::string_view AndroidSensitiveContentClient::GetHistogramPrefix() {
  return histogram_prefix_;
}

}  // namespace sensitive_content
