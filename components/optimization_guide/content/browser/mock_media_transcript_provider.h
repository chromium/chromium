// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_MEDIA_TRANSCRIPT_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_MEDIA_TRANSCRIPT_PROVIDER_H_

#include "components/optimization_guide/content/browser/media_transcript_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {

namespace proto {
class MediaTranscript;
}  // namespace proto

class MockMediaTranscriptProvider : public MediaTranscriptProvider {
 public:
  MockMediaTranscriptProvider();
  ~MockMediaTranscriptProvider() override;

  MOCK_METHOD(std::vector<proto::MediaTranscript>,
              GetTranscriptsForFrame,
              (content::RenderFrameHost*),
              (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_MEDIA_TRANSCRIPT_PROVIDER_H_
