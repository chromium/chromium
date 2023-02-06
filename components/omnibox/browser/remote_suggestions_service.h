// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/template_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

// A service to fetch suggestions from a search provider's suggest endpoint.
// Used by the ZeroSuggestProvider, the SearchProvider, and the ImageService.
//
// This service is always sent the user's authentication state, so the
// suggestions always can be personalized. This service is also sometimes sent
// the user's current URL, so the suggestions are sometimes also contextual.
class RemoteSuggestionsService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the transfer is about to start. `request_id` identifies the
    // request. `request` is deleted after this call once the transfer starts.
    virtual void OnSuggestRequestStarting(
        const base::UnguessableToken& request_id,
        const network::ResourceRequest* request) {}
    // Called when the transfer is done. `request_id` identifies the request.
    // `response_received` indicates whether the request has succeeded and
    // `response_body` is populated.
    virtual void OnSuggestRequestCompleted(
        const base::UnguessableToken& request_id,
        const bool response_received,
        const std::unique_ptr<std::string>& response_body) {}
  };

  explicit RemoteSuggestionsService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~RemoteSuggestionsService() override;
  RemoteSuggestionsService(const RemoteSuggestionsService&) = delete;
  RemoteSuggestionsService& operator=(const RemoteSuggestionsService&) = delete;

  // Called when the transfer is done. `response_received` indicates whether the
  // request has succeeded and `response_body` is populated.
  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              const bool response_received,
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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Called when the transfer is done. Notifies `observers_` and calls
  // `completion_callback`.
  void OnURLLoadComplete(const base::UnguessableToken& request_id,
                         CompletionCallback completion_callback,
                         const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Observers being notified of request start and completion events.
  base::ObserverList<Observer> observers_;
  // Used to bind `OnURLLoadComplete` to the network loader's callback as the
  // loader is no longer owned by `this` once returned.
  base::WeakPtrFactory<RemoteSuggestionsService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
