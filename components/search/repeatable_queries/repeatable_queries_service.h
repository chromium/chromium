// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_H_
#define COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search/repeatable_queries/repeatable_queries_service_observer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

class SearchProviderObserver;
class TemplateURLService;
class TemplateURL;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace history {
class HistoryService;
class URLDatabase;
}  // namespace history

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// Represents a repeatable query suggestion.
class RepeatableQuery {
 public:
  RepeatableQuery() = default;
  ~RepeatableQuery() = default;

  bool operator==(const RepeatableQuery& other) const {
    return query == other.query && destination_url == other.destination_url &&
           deletion_url == other.deletion_url;
  }
  bool operator!=(const RepeatableQuery& other) const {
    return !(this == &other);
  }

  // Repeatable query suggestion.
  base::string16 query;

  // The URL to navigate to when the suggestion is selected.
  GURL destination_url;

  // The relative endpoint used for deleting the query suggestion on the server.
  // Populated for server provided queries only.
  std::string deletion_url;
};

// Provides repeatable query suggestions to be shown in the NTP Most Visited
// tiles when Google is the default search provider. The repeatable queries are
// requested from the server for signed-in users and extracted from the
// in-memory URLDatabase for unauthenticated users.
class RepeatableQueriesService : public KeyedService {
 public:
  RepeatableQueriesService(
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& request_initiator_url);
  ~RepeatableQueriesService() override;
  RepeatableQueriesService(const RepeatableQueriesService&) = delete;
  RepeatableQueriesService& operator=(const RepeatableQueriesService&) = delete;

  // Histograms recorded by this class.
  static const char kExtractedCountHistogram[];
  static const char kExtractionDurationHistogram[];

  // KeyedService:
  void Shutdown() override;

  // Returns the currently cached repeatable query suggestions, if any.
  const std::vector<RepeatableQuery>& repeatable_queries() const;

  // If Google is the default search provider, asynchronously requests
  // repeatable query suggestions from the server for signed-in users and
  // synchronously extracts them from the in-memory URLDatabase for
  // unauthenticated users. Regardless of success, observers are notified via
  // RepeatableQueriesServiceObserver::OnRepeatableQueriesUpdated.
  void Refresh();

  // Deletes the records of the repeatable query suggestion with the given
  // destination URL on the server as well as on the device, whichever is
  // applicable. Prevents the suggestion from being offered again by
  // blocklisting it. Updates the current set of suggestions and notifies the
  // observers.
  void DeleteQueryWithDestinationURL(const GURL& url);

  // Add/remove observers.
  void AddObserver(RepeatableQueriesServiceObserver* observer);
  void RemoveObserver(RepeatableQueriesServiceObserver* observer);

 protected:
  class SigninObserver;

  virtual SigninObserver* signin_observer();
  virtual SearchProviderObserver* search_provider_observer();

  // Called when the default search provider changes.
  void SearchProviderChanged();

  // Called when the signin status changes.
  void SigninStatusChanged();

  // Returns the server destination URL for |query| with |search_provider|.
  // |search_provider| may not be nullptr.
  GURL GetQueryDestinationURL(const base::string16& query,
                              const TemplateURL* search_provider);

  // Returns the resolved deletion URL for the given relative deletion URL.
  GURL GetQueryDeletionURL(const std::string& deletion_url);

  // Returns the server request URL.
  GURL GetRequestURL();

  void FlushForTesting(base::OnceClosure flushed);

 private:
  // Requests repeatable queries from the server. Called for signed-in users.
  void GetRepeatableQueriesFromServer();
  void RepeatableQueriesResponseLoaded(network::SimpleURLLoader* loader,
                                       std::unique_ptr<std::string> response);
  void RepeatableQueriesParsed(data_decoder::DataDecoder::ValueOrError result);

  // Queries the in-memory URLDatabase for the repeatable queries submitted
  // to the default search provider. Called for unauthenticated users.
  void GetRepeatableQueriesFromURLDatabase();

  // Deletes |query| from the in-memory URLDatabase.
  void DeleteRepeatableQueryFromURLDatabase(const base::string16& query);
  void DeleteRepeatableQueryFromURLDatabaseTask(const base::string16& query,
                                                history::URLDatabase* url_db);

  // Deletes the query with |deletion_url| from the server.
  void DeleteRepeatableQueryFromServer(const std::string& deletion_url);
  void DeletionResponseLoaded(network::SimpleURLLoader* loader,
                              std::unique_ptr<std::string> response);

  void NotifyObservers();

  bool IsQueryDeleted(const base::string16& query);
  void MarkQueryAsDeleted(const base::string16& query);

  history::HistoryService* history_service_;

  TemplateURLService* template_url_service_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const GURL request_initiator_url_;

  std::unique_ptr<SigninObserver> signin_observer_;

  std::unique_ptr<SearchProviderObserver> search_provider_observer_;

  base::ObserverList<RepeatableQueriesServiceObserver, true> observers_;

  std::vector<RepeatableQuery> repeatable_queries_;

  // Used to ensure the deleted repeatable queries won't be suggested again.
  // This does not need to be persisted across sessions as the queries do get
  // deleted on the server as well as on the device, whichever is applicable.
  std::set<base::string16> deleted_repeatable_queries_;

  std::vector<std::unique_ptr<network::SimpleURLLoader>> loaders_;

  // The TaskRunner to which in-memory URLDatabase deletion tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> deletion_task_runner_;

  base::WeakPtrFactory<RepeatableQueriesService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_H_
