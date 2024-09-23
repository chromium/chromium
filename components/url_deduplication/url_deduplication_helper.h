// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_DEDUPLICATION_URL_DEDUPLICATION_HELPER_H_
#define COMPONENTS_URL_DEDUPLICATION_URL_DEDUPLICATION_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/url_strip_handler.h"

class GURL;

namespace url_deduplication {

class URLDeduplicationHelper {
 public:
  URLDeduplicationHelper(
      std::vector<std::unique_ptr<URLStripHandler>> strip_handlers,
      DeduplicationStrategy strategy);

  explicit URLDeduplicationHelper(DeduplicationStrategy strategy);

  ~URLDeduplicationHelper();

  // Returns a unique identifier for a given URL (i.e. deduplication key) such
  // that related URLs will generate the same key and so clients may recognize
  // that two similar looking URLs represent the same visit or visit intention.
  std::string ComputeURLDeduplicationKey(const GURL& url,
                                         const std::string& title);

  void AddStripHandler(std::unique_ptr<URLStripHandler> handler) {
    strip_handlers_.push_back(std::move(handler));
  }

 private:
  std::vector<std::unique_ptr<URLStripHandler>> strip_handlers_;
  DeduplicationStrategy strategy_;
};

}  // namespace url_deduplication

#endif  // COMPONENTS_URL_DEDUPLICATION_URL_DEDUPLICATION_HELPER_H_
