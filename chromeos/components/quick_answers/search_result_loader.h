// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/result_loader.h"
#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace quick_answers {

class SearchResultLoader : public ResultLoader {
 public:
  SearchResultLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ResultLoaderDelegate* delegate);

  SearchResultLoader(const SearchResultLoader&) = delete;
  SearchResultLoader& operator=(const SearchResultLoader&) = delete;

  ~SearchResultLoader() override;

  // ResultLoader:
  void BuildRequest(const PreprocessedOutput& preprocessed_output,
                    BuildRequestCallback callback) const override;
  void ProcessResponse(const PreprocessedOutput& preprocessed_output,
                       std::unique_ptr<std::string> response_body,
                       ResponseParserCallback complete_callback) override;

 private:
  std::unique_ptr<SearchResponseParser> search_response_parser_;
  base::WeakPtrFactory<SearchResultLoader> weak_ptr_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
