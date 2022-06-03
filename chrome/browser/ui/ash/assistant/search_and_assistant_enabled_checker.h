// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_SEARCH_AND_ASSISTANT_ENABLED_CHECKER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_SEARCH_AND_ASSISTANT_ENABLED_CHECKER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

// SearchAndAssistantEnabledChecker is the class that send HTTP request to sync
// the Search and Assistant state.
class SearchAndAssistantEnabledChecker {
 public:
  // A delegate interface for the SearchAndAssistantEnabledChecker.
  class Delegate {
   public:
    // Invoked when there is an error.
    virtual void OnError() {}

    // Invoked when the Search and Assistant bit is received.
    virtual void OnSearchAndAssistantStateReceived(bool is_disabled) {}

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  SearchAndAssistantEnabledChecker(
      network::mojom::URLLoaderFactory* url_loader_factory,
      Delegate* delegate);

  // Disallow copy and assign.
  SearchAndAssistantEnabledChecker(const SearchAndAssistantEnabledChecker&) =
      delete;
  SearchAndAssistantEnabledChecker& operator=(
      const SearchAndAssistantEnabledChecker&) = delete;

  ~SearchAndAssistantEnabledChecker();

  void SyncSearchAndAssistantState();

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError response);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  network::mojom::URLLoaderFactory* url_loader_factory_;
  Delegate* const delegate_;

  base::WeakPtrFactory<SearchAndAssistantEnabledChecker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_SEARCH_AND_ASSISTANT_ENABLED_CHECKER_H_
