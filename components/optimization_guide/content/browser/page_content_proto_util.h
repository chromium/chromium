// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_

#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"

namespace optimization_guide {

// Converts the mojom data structure for AIPageContent to its equivalent proto
// mapping.
void ConvertAIPageContentToProto(
    const blink::mojom::AIPageContent& mojo,
    optimization_guide::proto::AnnotatedPageContent* proto);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
