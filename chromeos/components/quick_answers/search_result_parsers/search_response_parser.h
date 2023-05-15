// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace base {
class Value;
}  // namespace base

namespace quick_answers {

// Parser for extracting quick answer result out of the search response.
class SearchResponseParser {
 public:
  // Callback used when parsing of `quick_answers_session` is complete. Note
  // that `quick_answers_session` may be `nullptr`.
  using SearchResponseParserCallback = base::OnceCallback<void(
      std::unique_ptr<QuickAnswersSession> quick_answers_session)>;

  explicit SearchResponseParser(SearchResponseParserCallback complete_callback);
  ~SearchResponseParser();

  SearchResponseParser(const SearchResponseParser&) = delete;
  SearchResponseParser& operator=(const SearchResponseParser&) = delete;

  // Starts processing the search response.
  void ProcessResponse(std::unique_ptr<std::string> response_body);

 private:
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  //  void OnJSONParseFailed(const std::string& error_message);

  std::unique_ptr<QuickAnswersSession> ProcessResult(const base::Value* result);

  SearchResponseParserCallback complete_callback_;

  base::WeakPtrFactory<SearchResponseParser> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_
