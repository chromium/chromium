// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/optimization_guide/content/browser/ai_page_content_metadata.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace content {
class WebContents;
}

namespace optimization_guide {
blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions();

// The result of a call to GetAIPageContent.  It contains the
// AnnotatedPageContent proto and metadata about the page content.
struct AIPageContentResult {
  AIPageContentResult();
  AIPageContentResult(const AIPageContentResult& other) = delete;
  AIPageContentResult(AIPageContentResult&& other);
  AIPageContentResult& operator=(const AIPageContentResult& other) = delete;
  AIPageContentResult& operator=(AIPageContentResult&& other);
  ~AIPageContentResult();

  optimization_guide::proto::AnnotatedPageContent proto;
  AIPageContentMetadata metadata;
};

// Provides AIPageContentResult (AnnotatedPageContent proto and metadata) for
// the primary page displayed in a WebContents.
using OnAIPageContentDone =
    base::OnceCallback<void(std::optional<AIPageContentResult>)>;
void GetAIPageContent(content::WebContents* web_contents,
                      blink::mojom::AIPageContentOptionsPtr options,
                      OnAIPageContentDone done_callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
