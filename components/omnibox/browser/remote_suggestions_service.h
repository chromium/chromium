// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"
#include "components/search_engines/template_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

class DocumentSuggestionsService;
class EnterpriseSearchAggregatorSuggestionsService;

using EnterpriseSearchAggregatorSuggestionType =
    AutocompleteMatch::EnterpriseSearchAggregatorType;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

// The types of requests for remote suggestions.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Must be kept in sync with RemoteRequestType enum and variant.
// LINT.IfChange(RemoteRequestType)
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
  // Enterprise Search Aggregator suggestion requests.
  kEnterpriseSearchAggregatorSuggest = 7,
  kMaxValue = kEnterpriseSearchAggregatorSuggest,
};
// LINT.ThenChange(
//     //tools/metrics/histograms/metadata/omnibox/enums.xml:RemoteRequestType,
//     //tools/metrics/histograms/metadata/omnibox/histograms.xml:RemoteRequestType
// )

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
    virtual void OnRequestCreated(const base::UnguessableToken& request_id,
                                  const network::ResourceRequest* request) {}
    // Called when the transfer has started. `request_id` identifies the
    // request. `request_body` is the HTTP POST upload body, if applicable.
    virtual void OnRequestStarted(const base::UnguessableToken& request_id,
                                  network::SimpleURLLoader* loader,
                                  const std::string& request_body) {}
    // Called when the transfer is done. `request_id` identifies the request.
    // `response_code` is the response status code. A status code of 200
    // indicates that the request has succeeded and `response_body` is
    // populated.
    virtual void OnRequestCompleted(
        const base::UnguessableToken& request_id,
        const int response_code,
        const std::unique_ptr<std::string>& response_body) {}
  };

  // Called when the transfer has started asynchronously, e.g., after obtaining
  // an OAuth token.
  using StartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;
  // Called when the transfer is done. `response_code` is the response status
  // code. A status code of 200 indicates that the request has succeeded and
  // `response_body` is populated.
  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              const int response_code,
                              std::unique_ptr<std::string> response_body)>;
  // Same as `StartCallback` but for requests that are associated with a
  // `request_index`.
  using IndexedStartCallback = base::RepeatingCallback<void(
      const int request_index,
      std::unique_ptr<network::SimpleURLLoader> loader)>;
  // Same as `CompletionCallback` but for requests that are associated with a
  // `request_index`.
  using IndexedCompletionCallback =
      base::RepeatingCallback<void(const int request_index,
                                   const network::SimpleURLLoader* source,
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
    virtual void OnRequestCompleted(const network::SimpleURLLoader* source,
                                    const int response_code,
                                    std::unique_ptr<std::string> response_body,
                                    CompletionCallback completion_callback) = 0;

    virtual void OnIndexedRequestCompleted(
        const int request_index,
        const network::SimpleURLLoader* source,
        const int response_code,
        std::unique_ptr<std::string> response_body,
        IndexedCompletionCallback completion_callback) = 0;

   protected:
    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  RemoteSuggestionsService(
      DocumentSuggestionsService* document_suggestions_service,
      EnterpriseSearchAggregatorSuggestionsService*
          enterprise_search_aggregator_suggestions_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~RemoteSuggestionsService() override;
  RemoteSuggestionsService(const RemoteSuggestionsService&) = delete;
  RemoteSuggestionsService& operator=(const RemoteSuggestionsService&) = delete;

  // Helper to set the time request of type `request_type` has started in
  // `time_request_sent_`.
  void SetTimeRequestSent(RemoteRequestType request_type, base::TimeTicks time);

  // Logs how long it has been since a request started at `start_time` sliced by
  // whether the request was completed or interrupted.
  void LogResponseTime(RemoteRequestType request_type, bool interrupted);

  // Returns the suggest endpoint URL for `template_url`.
  // `search_terms_args` is used to build the endpoint URL.
  // `search_terms_data` is used to build the endpoint URL.
  static GURL EndpointUrl(
      const TemplateURL& template_url,
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
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
      bool is_off_the_record,
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
      bool is_off_the_record,
      const TemplateURL* template_url,
      TemplateURLRef::SearchTermsArgs search_terms_args,
      const SearchTermsData& search_terms_data,
      CompletionCallback completion_callback);

  // Creates and starts a document suggestion request for `query` asynchronously
  // after obtaining an OAuth2 token for the signed-in users.
  void CreateDocumentSuggestionsRequest(
      const std::u16string& query,
      bool is_off_the_record,
      metrics::OmniboxEventProto::PageClassification page_classification,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Stops creating the request. Already created requests aren't affected.
  void StopCreatingDocumentSuggestionsRequest();

  // Creates and starts an enterprise search aggregator suggestion request using
  //  `suggest_url` and `response_body` asynchronously after obtaining an OAuth2
  //  token for signed-in enterprise users.
  void CreateEnterpriseSearchAggregatorSuggestionsRequest(
      const std::u16string& query,
      const GURL& suggest_url,
      metrics::OmniboxEventProto::PageClassification page_classification,
      IndexedStartCallback start_callback,
      IndexedCompletionCallback completion_callback,
      std::vector<std::vector<int>> suggestion_types);

  // Stops creating the request. Already created requests aren't affected.
  void StopCreatingEnterpriseSearchAggregatorSuggestionsRequest();

  // Creates and returns a loader to delete personalized suggestions.
  //
  // `deletion_url` must be a valid URL.
  // `completion_callback` will be invoked when the transfer is done.
  std::unique_ptr<network::SimpleURLLoader> StartDeletionRequest(
      const std::string& deletion_url,
      bool is_off_the_record,
      CompletionCallback completion_callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetDelegate(base::WeakPtr<Delegate> delegate);

  // Exposed for testing.
  void set_url_loader_factory_for_testing(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  // Called when the request has been created, before the transfer has started.
  // Notifies `observers_`.
  void OnRequestCreated(const base::UnguessableToken& request_id,
                        network::ResourceRequest* request);
  // Called when the transfer has started. Notifies `observers_`.
  void OnRequestStarted(
      const base::UnguessableToken& request_id,
      RemoteRequestType request_type,
      metrics::OmniboxEventProto::PageClassification page_classification,
      network::SimpleURLLoader* loader,
      const std::string& request_body);
  // Called when the transfer has started asynchronously, e.g., after obtaining
  // an OAuth token. Notifies `observers_` and calls `start_callback` to
  // transfer the ownership of `loader` to the caller.
  void OnRequestStartedAsync(
      const base::UnguessableToken& request_id,
      RemoteRequestType request_type,
      metrics::OmniboxEventProto::PageClassification page_classification,
      StartCallback start_callback,
      std::unique_ptr<network::SimpleURLLoader> loader,
      const std::string& request_body);
  void OnIndexedRequestStartedAsync(
      const base::UnguessableToken& request_id,
      RemoteRequestType request_type,
      metrics::OmniboxEventProto::PageClassification page_classification,
      IndexedStartCallback start_callback,
      int request_index,
      std::unique_ptr<network::SimpleURLLoader> loader,
      const std::string& request_body);
  // Called when the transfer is done. Notifies `observers_` and calls
  // `completion_callback` passing the response to the caller.
  void OnRequestCompleted(
      const base::UnguessableToken& request_id,
      RemoteRequestType request_type,
      base::ElapsedTimer request_timer,
      metrics::OmniboxEventProto::PageClassification page_classification,
      CompletionCallback completion_callback,
      const network::SimpleURLLoader* source,
      std::unique_ptr<std::string> response_body);

  void OnIndexedRequestCompleted(
      const base::UnguessableToken& request_id,
      RemoteRequestType request_type,
      metrics::OmniboxEventProto::PageClassification page_classification,
      base::TimeTicks start_time,
      IndexedCompletionCallback completion_callback,
      const network::SimpleURLLoader* source,
      int request_index,
      std::unique_ptr<std::string> response_body);

  // May be nullptr in OTR profiles. Otherwise guaranteed to outlive this due to
  // the factories' dependency.
  raw_ptr<DocumentSuggestionsService> document_suggestions_service_;
  // May be nullptr in OTR profiles. Otherwise guaranteed to outlive this due to
  // the factories' dependency.
  raw_ptr<EnterpriseSearchAggregatorSuggestionsService>
      enterprise_search_aggregator_suggestions_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Time request sent for each RemoteRequestType. Used for histogram logging.
  std::map<RemoteRequestType, base::TimeTicks> time_request_sent_;
  // Observers being notified of request start and completion events.
  base::ObserverList<Observer> observers_;
  // Delegate to which invocation of completion callback is delegated.
  base::WeakPtr<Delegate> delegate_;
  // Used to bind `OnURLLoadComplete` to the network loader's callback as the
  // loader is no longer owned by `this` once returned.
  base::WeakPtrFactory<RemoteSuggestionsService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_REMOTE_SUGGESTIONS_SERVICE_H_
