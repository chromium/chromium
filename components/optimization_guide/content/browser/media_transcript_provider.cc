// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/media_transcript_provider.h"

#include "content/public/browser/web_contents.h"

namespace optimization_guide {

namespace {

const char kMediaTranscriptProviderKey[] = "MediaTranscriptProvider";

}  // namespace

// static
MediaTranscriptProvider* MediaTranscriptProvider::GetFor(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return static_cast<MediaTranscriptProvider*>(
      web_contents->GetUserData(UserDataKey()));
}

// static
void MediaTranscriptProvider::SetFor(
    content::WebContents* web_contents,
    std::unique_ptr<MediaTranscriptProvider> provider) {
  CHECK(web_contents);
  web_contents->SetUserData(UserDataKey(), std::move(provider));
}

// static
const void* MediaTranscriptProvider::UserDataKey() {
  return &kMediaTranscriptProviderKey;
}

}  // namespace optimization_guide
