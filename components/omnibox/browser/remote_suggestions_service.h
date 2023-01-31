// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/template_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// A service to fetch suggestions from the default search provider's suggest
// service. In practice, the usage of this service is inconsistent.
//  - Users: ZeroSuggest, ZeroSuggest-prefetch, EntityImageService.
//  - Non-users: SearchProvider.
//
// This service is always sent the user's authentication state, so the
// suggestions always can be personalized. This service is also sometimes sent
// the user's current URL, so the suggestions are sometimes also contextual.
class RemoteSuggestionsService : public KeyedService {
 public:
  explicit RemoteSuggestionsService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~RemoteSuggestionsService() override;
  RemoteSuggestionsService(const RemoteSuggestionsService&) = delete;
  RemoteSuggestionsService& operator=(const RemoteSuggestionsService&) = delete;

  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  // Returns the suggest endpoint URL for `template_url`.
  //
  // `template_url` must not be nullptr.
  // `search_terms_args` is used to build the endpoint URL.
  // `search_terms_data` is used to build the endpoint URL.
  static GURL EndpointUrl(const TemplateURL* template_url,
                          TemplateURLRef::SearchTermsArgs search_terms_args,
                          const SearchTermsData& search_terms_data);

  // Creates and returns a loader for remote suggestions for `template_url`.
  // It uses a number of signals to create the loader, including field trial
  // parameters.
  //
  // `template_url` must not be nullptr.
  // `search_terms_args` is used to build the endpoint URL.
  // `search_terms_data` is used to build the endpoint URL.
  // `completion_callback` will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartSuggestionsRequest(
      const TemplateURL* template_url,
      TemplateURLRef::SearchTermsArgs search_terms_args,
      const SearchTermsData& search_terms_data,
      CompletionCallback completion_callback);

  // Creates and returns a loader for remote zero-prefix suggestions for
  // `template_url`. It uses a number of signals to create the loader, including
  // field trial parameters.
  //
  // `template_url` must not be nullptr.
  // `search_terms_args` is used to build the endpoint URL.
  // `search_terms_data` is used to build the endpoint URL.
  // `completion_callback` will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartZeroPrefixSuggestionsRequest(
      const TemplateURL* template_url,
      TemplateURLRef::SearchTermsArgs search_terms_args,
      const SearchTermsData& search_terms_data,
      CompletionCallback completion_callback);

  // Creates and returns a loader to delete personalized suggestions.
  //
  // `deletion_url` must be a valid URL.
  // `completion_callback` will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartDeletionRequest(
      const std::string& deletion_url,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
