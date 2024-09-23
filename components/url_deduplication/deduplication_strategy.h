// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_DEDUPLICATION_DEDUPLICATION_STRATEGY_H_
#define COMPONENTS_URL_DEDUPLICATION_DEDUPLICATION_STRATEGY_H_

#include <string>
#include <vector>

namespace url_deduplication {

class DeduplicationStrategy {
 public:
  DeduplicationStrategy();

  DeduplicationStrategy(const DeduplicationStrategy& strategy);

  ~DeduplicationStrategy();

  // Vector of prefixes to remove from urls.
  std::vector<std::string> excluded_prefixes{{}};
  // Param to determine if https should be replaced with http.
  bool update_scheme{false};
  // Param to determine if username should be cleared.
  bool clear_username{false};
  // Param to determine if password should be cleared.
  bool clear_password{false};
  // Param to determine if the URL path should be cleared.
  bool clear_path{false};
  // Param to determine if query should be cleared.
  bool clear_query{false};
  // Param to determine if ref should be cleared.
  bool clear_ref{false};
  // Param to determine if port should be cleared.
  bool clear_port{false};
  // Param  to determine if the page title should be included when computing the
  // URL merge key for a given URL.
  bool include_title{false};
};

}  // namespace url_deduplication

#endif  // COMPONENTS_URL_DEDUPLICATION_DEDUPLICATION_STRATEGY_H_
