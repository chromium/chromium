// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
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

class TemplateURLService;

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

  // Returns a URL representing the address of the server where the zero suggest
  // request is being sent. Does not take into account whether sending this
  // request is prohibited (e.g. in an incognito window).
  // Returns an invalid URL (i.e.: GURL::is_valid() == false) in case of an
  // error.
  //
  // |search_terms_args| encapsulates the arguments sent to the suggest service.
  // Various parts of it (including the current page URL and classification) are
  // used to build the final endpoint URL. Note that the current page URL can
  // be empty.
  //
  // Note that this method is public and is also used by ZeroSuggestProvider for
  // suggestions that do not take the current page URL into consideration.
  static GURL EndpointUrl(TemplateURLRef::SearchTermsArgs search_terms_args,
                          const TemplateURLService* template_url_service);

  // Creates and returns a loader for remote suggestions for |search_terms_args|
  // and passes the loader to |start_callback|. It uses a number of signals to
  // create the loader, including field trial / experimental parameters.
  //
  // |search_terms_args| encapsulates the arguments sent to the remote service.
  // If |search_terms_args.current_page_url| is empty, the system will never use
  // the experimental suggestions service. It's possible the non-experimental
  // service may decide to offer general-purpose suggestions.
  //
  // |template_url_service| may be null, but some services may be disabled.
  //
  // |completion_callback| will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartSuggestionsRequest(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const TemplateURLService* template_url_service,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
