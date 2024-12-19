// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_

#include "base/functional/callback.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

namespace content {
class WebContents;
}

namespace optimization_guide {

// Provides AIPageContent representation for the primary page displayed in a
// WebContents.
using OnAIPageContentDone = base::OnceCallback<void(
    std::optional<optimization_guide::proto::AnnotatedPageContent>)>;
void GetAIPageContent(content::WebContents* web_contents,
                      OnAIPageContentDone done_callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
