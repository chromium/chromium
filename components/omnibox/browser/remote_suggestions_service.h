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

class DocumentSuggestionsService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

// The types of requests for remote suggestions.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class RemoteRequestType {
  // Search suggestion requests.
  kSearch = 0,
  // Search suggestion warm-up requests.
  kSearchWarmup = 1,
  // Search suggestion requests to obtain images.
  kImages = 2,
  // Zero-prefix suggestion requests.
  kZeroSuggest = 3,
  // Zero-prefix suggestion prefetching requests.
  kZeroSuggestPrefetch = 4,
  // Document suggestion requests.
  kDocumentSuggest = 5,
  // Suggestion deletion requests.
  kDeletion = 6,
  kMaxValue = kDeletion,
};

// The event types recorded by the providers for remote suggestions. Each event
// must be logged at most once from when the provider is started until it is
// stopped.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class RemoteRequestEvent {
  // Cached response was synchronously converted to displayed matches.
  // Recorded for non-prefetch requests only.
  kCachedResponseConvertedToMatches = 0,
  // Request was sent.
  kRequestSent = 1,
  // Request was invalidated.
  kRequestInvalidated = 2,
  // Response was received asynchronously.
  kResponseReceived = 3,
  // Response was cached.
  kResponseCached = 4,
  // Response ended up being converted to displayed matches. This may happen due
  // to an empty displayed result set or an empty remote result set.
  // Recorded for non-prefetch requests only.
  kResponseConvertedToMatches = 5,
  kMaxValue = kResponseConvertedToMatches,
};

// A service to fetch suggestions from a search provider's suggest endpoint.
// Used by ZeroSuggestProvider, SearchProvider, DocumentProvider, and
// ImageService.
//
// This service is always sent the user's authentication state, so the
// suggestions always can be personalized. This service is also sometimes sent
// the user's current URL, so the suggestions are sometimes also contextual.
class RemoteSuggestionsService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the request has been created. `request_id` identifies the
    // request. `request` is deleted after this call once the transfer starts.
    virtual void OnSuggestRequestCreated(
        const base::UnguessableToken& request_id,
        const network::ResourceRequest* request) {}
    // Called when the transfer has started. `request_id` identifies the
    // request. `request_body` is the HTTP POST upload body, if applicable.
    virtual void OnSuggestRequestStarted(
        const base::UnguessableToken& request_id,
        network::SimpleURLLoader* loader,
        const std::string& request_body) {}
    // Called when the transfer is done. `request_id` identifies the request.
    // `response_code` is the response status code. A status code of 200
    // indicates that the request has succeeded and `response_body` is
    // populated.
    virtual void OnSuggestRequestCompleted(
        const base::UnguessableToken& request_id,
        const int response_code,
        const std::unique_ptr<std::string>& response_body) {}
  };

  // Called when the transfer is done. `response_code` is the response status
  // code. A status code of 200 indicates that the request has succeeded and
  // `response_body` is populated.
  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              const int response_code,
                              std::unique_ptr<std::string> response_body)>;

  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called when the transfer is done. Delegates invocation of
    // `completion_callback`
    virtual void OnSuggestRequestCompleted(
        const network::SimpleURLLoader* source,
        const int response_code,
        std::unique_ptr<std::string> response_body,
        CompletionCallback completion_callback) = 0;

   protected:
    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  RemoteSuggestionsService(
      DocumentSuggestionsService* document_suggestions_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~RemoteSuggestionsService() override;
  RemoteSuggestionsService(const RemoteSuggestionsService&) = delete;
  RemoteSuggestionsService& operator=(const RemoteSuggestionsService&) = delete;

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
      RemoteRequestType request_type,
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
      RemoteRequestType request_type,
      const TemplateURL* template_url,
      TemplateURLRef::SearchTermsArgs search_terms_args,
      const SearchTermsData& search_terms_data,
      CompletionCallback completion_callback);

  // Creates and starts a document suggestion request for |query|.
  // May obtain an OAuth2 token for the signed-in users.
  using DocumentStartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;
  void CreateDocumentSuggestionsRequest(const std::u16string& query,
                                        bool is_incognito,
                                        DocumentStartCallback start_callback,
                                        CompletionCallback completion_callback);

  // Advises the service to stop any process that creates a document suggestion
  // request.
  void StopCreatingDocumentSuggestionsRequest();

  // Creates and returns a loader to delete personalized suggestions.
  //
  // `deletion_url` must be a valid URL.
  // `completion_callback` will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartDeletionRequest(
      const std::string& deletion_url,
      CompletionCallback completion_callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetDelegate(base::WeakPtr<Delegate> delegate);

  // Exposed for testing.
  void set_url_loader_factory_for_testing(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  // Called when the request has been created, before fetching the OAuth2 token.
  // Notifies `observers_`.
  void OnDocumentSuggestionsRequestAvailable(
      const base::UnguessableToken& request_id,
      network::ResourceRequest* request);
  // Called when the transfer has started, after receiving the OAuth2 token.
  // Notifies `observers_` and calls `start_callback`.
  void OnDocumentSuggestionsLoaderAvailable(
      const base::UnguessableToken& request_id,
      DocumentStartCallback start_callback,
      std::unique_ptr<network::SimpleURLLoader> loader,
      const std::string& request_body);
  // Called when the transfer is done. Notifies `observers_` and calls
  // `completion_callback`.
  void OnURLLoadComplete(const base::UnguessableToken& request_id,
                         CompletionCallback completion_callback,
                         const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  raw_ptr<DocumentSuggestionsService> document_suggestions_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Observers being notified of request start and completion events.
  base::ObserverList<Observer> observers_;
  // Delegate to which invocation of completion callback is delegated.
  base::WeakPtr<Delegate> delegate_;
  // Used to bind `OnURLLoadComplete` to the network loader's callback as the
  // loader is no longer owned by `this` once returned.
  base::WeakPtrFactory<RemoteSuggestionsService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
