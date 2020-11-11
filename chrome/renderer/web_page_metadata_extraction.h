// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEB_PAGE_METADATA_EXTRACTION_H_
#define CHROME_RENDERER_WEB_PAGE_METADATA_EXTRACTION_H_

#include "chrome/common/web_page_metadata.mojom-forward.h"

namespace blink {
class WebLocalFrame;
}

namespace chrome {

// Extracts any metadata information out of the document in the |frame|. Note,
// this function always returns a non-null object even if no metadata was found
// in the page.
mojom::WebPageMetadataPtr ExtractWebPageMetadata(blink::WebLocalFrame* frame);

}  // namespace chrome

#endif  // CHROME_RENDERER_WEB_PAGE_METADATA_EXTRACTION_H_
