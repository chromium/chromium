// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_FILTER_TOOL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_FILTER_TOOL_H_

#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace url_pattern_index {
namespace flat {
struct UrlRule;
}
}  // namespace url_pattern_index

namespace subresource_filter {

// FilterTool provides utility functions for matching a given ruleset against
// requests and writes the results to an output stream.
class FilterTool {
 public:
  // |output| must outlive this object.
  FilterTool(
      scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset,
      std::ostream* output);

  FilterTool(const FilterTool&) = delete;
  FilterTool& operator=(const FilterTool&) = delete;

  ~FilterTool();

  // Checks the ruleset for a request with document origin |document_origin|,
  // sub-resource request |url|, and |type|. If a blocklist rule matches the
  // request, it is considered the match. If multiple blocklist rules match,
  // one is arbitrarily chosen as the match. If a blocklist rule matches and a
  // allowlist rule matches, an allowlist rule is the match. The output is
  // written to |output_| in a space- delimited line. The first column is
  // either BLOCKED or ALLOWED. The second is any matching rule. The following
  // columns are the input arguments.
  void Match(const std::string& document_origin,
             const std::string& url,
             const std::string& type);

  // Like Match, but for multiple requests. The requests are provided in
  // |request_stream| in form: "document_origin url type\n".
  void MatchBatch(std::istream* request_stream);

  // Like Match, but instead of writing the result of each request, it writes
  // the set of matched rules and their match counts (in descending order) to
  // |output_|. Use |min_match_count| to filter the list of written rules to
  // those that were matched at least |min_match_count| times.
  void MatchRules(std::istream* request_stream, int min_match_count);

 private:
  void PrintResult(bool blocked,
                   const url_pattern_index::flat::UrlRule* rule,
                   std::string_view document_origin,
                   std::string_view url,
                   std::string_view type);

  const url_pattern_index::flat::UrlRule* MatchImpl(
      std::string_view document_origin,
      std::string_view url,
      std::string_view type,
      bool* blocked);

  void MatchBatchImpl(std::istream* request_stream,
                      bool print_each_request,
                      int min_match_count);

  scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset_;
  std::ostream* output_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_FILTER_TOOL_H_
