// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_
#define COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_

#include "components/url_deduplication/url_strip_handler.h"
#include "url/gurl.h"

class DocsURLStripHandler : public URLStripHandler {
 public:
  DocsURLStripHandler();

  DocsURLStripHandler(const DocsURLStripHandler&) = delete;
  DocsURLStripHandler& operator=(const DocsURLStripHandler&) = delete;

  ~DocsURLStripHandler();
  // URLStripHandler:
  GURL StripExtraParams(GURL);
};

#endif  // COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_
