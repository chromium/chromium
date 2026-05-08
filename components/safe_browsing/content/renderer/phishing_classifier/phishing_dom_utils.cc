// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_utils.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace safe_browsing {

PhishingProcessStatus CanPerformPhishingDetection(blink::WebLocalFrame* frame) {
  if (!frame) {
    return PhishingProcessStatus::kInvalidDomLoader;
  }
  GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return PhishingProcessStatus::kInvalidUrlFormat;
  }

  blink::WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (!document_loader || document_loader->HttpMethod().Ascii() != "GET") {
    return PhishingProcessStatus::kInvalidDomLoader;
  }

  return PhishingProcessStatus::kValid;
}

}  // namespace safe_browsing
