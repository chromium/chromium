// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/template_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class TemplateURLService;

// Used by ZeroSuggestProvider only. This does duplicate some logic found in
// SearchProvider already. As-you-type suggest does NOT use this.
//
// TODO(tommycli): This class used to be much larger. At this point, it may make
// sense to to just fold it into ZeroSuggestProvider - or rename this class to
// include ZeroSuggest in the name.
//
// A service to fetch suggestions from the default search provider's suggest
// service.
//
// This service is always sent the user's authentication state, so the
// suggestions always can be personalized. This service is also sometimes sent
// the user's current URL, so the suggestions are sometimes also contextual.
class RemoteSuggestionsService : public KeyedService {
 public:
  explicit RemoteSuggestionsService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~RemoteSuggestionsService() override;

  using StartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;

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

  // Creates a loader for remote suggestions for |search_terms_args| and passes
  // the loader to |start_callback|. It uses a number of signals to create the
  // loader, including field trial / experimental parameters, and it may
  // pass a nullptr to |start_callback| (see below for details).
  //
  // |search_terms_args| encapsulates the arguments sent to the remote service.
  // If |search_terms_args.current_page_url| is empty, the system will never use
  // the experimental suggestions service. It's possible the non-experimental
  // service may decide to offer general-purpose suggestions.
  //
  // |template_url_service| may be null, but some services may be disabled.
  //
  // |start_callback| is called to transfer ownership of the created loader to
  //  whatever function/class receives the callback.
  //
  // |completion_callback| will be invoked when the transfer is done.
  void CreateSuggestionsRequest(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      const TemplateURLService* template_url_service,
      StartCallback start_callback,
      CompletionCallback completion_callback);

 private:
  // Activates a loader for |request|, wiring it up to |completion_callback|,
  // and calls |start_callback|.  If |request_body| isn't empty, it will be
  // attached as upload bytes.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsService);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
