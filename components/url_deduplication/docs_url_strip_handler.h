// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_
#define COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_

#include "components/url_deduplication/url_strip_handler.h"
#include "url/gurl.h"

namespace url_deduplication {

class DocsURLStripHandler : public URLStripHandler {
 public:
  DocsURLStripHandler() = default;

  DocsURLStripHandler(const DocsURLStripHandler&) = delete;
  DocsURLStripHandler& operator=(const DocsURLStripHandler&) = delete;

  ~DocsURLStripHandler() override = default;
  // URLStripHandler:
  GURL StripExtraParams(GURL) override;
};

}  // namespace url_deduplication

#endif  // COMPONENTS_URL_DEDUPLICATION_DOCS_URL_STRIP_HANDLER_H_
