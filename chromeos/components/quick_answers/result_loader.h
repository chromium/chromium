// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_RESULT_LOADER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_RESULT_LOADER_H_

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

class GURL;

namespace network {
class SimpleURLLoader;

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace chromeos {
namespace quick_answers {

enum class IntentType;

class ResultLoader {
 public:
  // A delegate interface for the ResultLoader.
  class ResultLoaderDelegate {
   public:
    ResultLoaderDelegate(const ResultLoaderDelegate&) = delete;
    ResultLoaderDelegate& operator=(const ResultLoaderDelegate&) = delete;

    // Invoked when there is a network error.
    virtual void OnNetworkError() {}

    // Invoked when the |quick_answer| is received. Note that |quick_answer| may
    // be |nullptr| if no answer found for the selected content.
    virtual void OnQuickAnswerReceived(
        std::unique_ptr<QuickAnswer> quick_answer) {}

   protected:
    ResultLoaderDelegate() = default;
    virtual ~ResultLoaderDelegate() = default;
  };

  // Callback used when parsing of |quick_answer| is complete. Note that
  // |quick_answer| may be |nullptr|.
  using ResponseParserCallback =
      base::OnceCallback<void(std::unique_ptr<QuickAnswer> quick_answer)>;

  ResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
               ResultLoaderDelegate* delegate);

  ResultLoader(const ResultLoader&) = delete;
  ResultLoader& operator=(const ResultLoader&) = delete;

  // Virtual for testing.
  virtual ~ResultLoader();

  // Creates ResultLoader based on the |intent_type|.
  static std::unique_ptr<ResultLoader> Create(
      IntentType intent_type,
      network::mojom::URLLoaderFactory* url_loader_factory,
      ResultLoader::ResultLoaderDelegate* delegate);

  // Starts downloading of |quick_answers| associated with |selected_text|,
  // calling |ResultLoaderDelegate| methods when finished.
  // Note that delegate methods should be called only once per
  // ResultLoader instance. Virtual for testing.
  virtual void Fetch(const std::string& selected_text);

 protected:
  // Builds the request URL from |selected_text|.
  virtual GURL BuildRequestUrl(const std::string& selected_text) const = 0;

  // Process the |response_body| and invoked the callback with |QuickAnswer|.
  virtual void ProcessResponse(std::unique_ptr<std::string> response_body,
                               ResponseParserCallback complete_callback) = 0;

 private:
  network::mojom::URLLoaderFactory* network_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  ResultLoaderDelegate* const delegate_;

  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnResultParserComplete(std::unique_ptr<QuickAnswer> quick_answer);

  // Time when the query is issued.
  base::TimeTicks fetch_start_time_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_RESULT_LOADER_H_
