// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MEDIA_TRANSCRIPT_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MEDIA_TRANSCRIPT_PROVIDER_H_

#include "base/supports_user_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {

// This interface provides media transcripts for a given `WebContents` if there
// is an active media session in it.
class MediaTranscriptProvider : public base::SupportsUserData::Data {
 public:
  MediaTranscriptProvider() = default;
  ~MediaTranscriptProvider() override = default;

  // Gets the `MediaTranscriptProvider` for the given `WebContents`.
  // This should only be called after `GlicMediaIntegration::GetFor()` has
  // been called for the given `WebContents`.
  static MediaTranscriptProvider* GetFor(content::WebContents* web_contents);

  // Sets the `MediaTranscriptProvider` for the given `WebContents`. Takes
  // ownership of the provider.
  static void SetFor(content::WebContents* web_contents,
                     std::unique_ptr<MediaTranscriptProvider> provider);

  // Returns the media transcripts captured in the given `RenderFrameHost`.
  virtual std::vector<optimization_guide::proto::MediaTranscript>
  GetTranscriptsForFrame(content::RenderFrameHost* rfh) = 0;

 private:
  // The key for storing the `MediaTranscriptProvider` in a `WebContents`.
  static const void* UserDataKey();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MEDIA_TRANSCRIPT_PROVIDER_H_
