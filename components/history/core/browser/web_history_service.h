// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class DictionaryValue;
}

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace version_info {
enum class Channel;
}

namespace history {

class WebHistoryServiceObserver;

// Provides an API for querying Google servers for a signed-in user's
// synced history visits. It is roughly analogous to HistoryService, and
// supports a similar API.
class WebHistoryService : public KeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

    // Returns true if the request is "pending" (i.e., it has been started, but
    // is not yet been complete).
    virtual bool IsPending() = 0;

    // Returns the response code received from the server, which will only be
    // valid if the request succeeded.
    virtual int GetResponseCode() = 0;

    // Returns the contents of the response body received from the server.
    virtual const std::string& GetResponseBody() = 0;

    virtual void SetPostData(const std::string& post_data) = 0;

    virtual void SetPostDataAndType(const std::string& post_data,
                                    const std::string& mime_type) = 0;

    virtual void SetUserAgent(const std::string& user_agent) = 0;

    // Tells the request to begin.
    virtual void Start() = 0;

   protected:
    Request();
  };

  // Callback with the result of a call to QueryHistory(). Currently, the
  // DictionaryValue is just the parsed JSON response from the server.
  // TODO(dubroy): Extract the DictionaryValue into a structured results object.
  typedef base::Callback<void(Request*, const base::DictionaryValue*)>
      QueryWebHistoryCallback;

  typedef base::Callback<void(bool success)> ExpireWebHistoryCallback;

  typedef base::Callback<void(bool success, bool new_enabled_value)>
      AudioWebHistoryCallback;

  typedef base::Callback<void(bool success)> QueryWebAndAppActivityCallback;

  typedef base::Callback<void(bool success)>
      QueryOtherFormsOfBrowsingHistoryCallback;

  typedef base::Callback<void(Request*, bool success)> CompletionCallback;

  WebHistoryService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WebHistoryService() override;

  void AddObserver(WebHistoryServiceObserver* observer);
  void RemoveObserver(WebHistoryServiceObserver* observer);

  // Searches synced history for visits matching |text_query|. The timeframe to
  // search, along with other options, is specified in |options|. If
  // |text_query| is empty, all visits in the timeframe will be returned.
  // This method is the equivalent of HistoryService::QueryHistory.
  // The caller takes ownership of the returned Request. If it is destroyed, the
  // request is cancelled.
  std::unique_ptr<Request> QueryHistory(
      const base::string16& text_query,
      const QueryOptions& options,
      const QueryWebHistoryCallback& callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Removes all visits to specified URLs in specific time ranges.
  // This is the of equivalent HistoryService::ExpireHistory().
  void ExpireHistory(const std::vector<ExpireHistoryArgs>& expire_list,
                     const ExpireWebHistoryCallback& callback,
                     const net::PartialNetworkTrafficAnnotationTag&
                         partial_traffic_annotation);

  // Removes all visits to specified URLs in the given time range.
  // This is the of equivalent HistoryService::ExpireHistoryBetween().
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time,
                            base::Time end_time,
                            const ExpireWebHistoryCallback& callback,
                            const net::PartialNetworkTrafficAnnotationTag&
                                partial_traffic_annotation);

  // Requests whether audio history recording is enabled.
  virtual void GetAudioHistoryEnabled(
      const AudioWebHistoryCallback& callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Sets the state of audio history recording to |new_enabled_value|.
  virtual void SetAudioHistoryEnabled(
      bool new_enabled_value,
      const AudioWebHistoryCallback& callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Queries whether web and app activity is enabled on the server.
  virtual void QueryWebAndAppActivity(
      const QueryWebAndAppActivityCallback& callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Used for tests.
  size_t GetNumberOfPendingAudioHistoryRequests();

  // Whether there are other forms of browsing history stored on the server.
  void QueryOtherFormsOfBrowsingHistory(
      version_info::Channel channel,
      const QueryOtherFormsOfBrowsingHistoryCallback& callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

 protected:
  // This function is pulled out for testing purposes. Caller takes ownership of
  // the new Request.
  virtual Request* CreateRequest(const GURL& url,
                                 const CompletionCallback& callback,
                                 const net::PartialNetworkTrafficAnnotationTag&
                                     partial_traffic_annotation);

  // Extracts a JSON-encoded HTTP response into a DictionaryValue.
  // If |request|'s HTTP response code indicates failure, or if the response
  // body is not JSON, a null pointer is returned.
  static std::unique_ptr<base::DictionaryValue> ReadResponse(Request* request);

  // Called by |request| when a web history query has completed. Unpacks the
  // response and calls |callback|, which is the original callback that was
  // passed to QueryHistory().
  static void QueryHistoryCompletionCallback(
      const WebHistoryService::QueryWebHistoryCallback& callback,
      WebHistoryService::Request* request,
      bool success);

  // Called by |request| when a request to delete history from the server has
  // completed. Unpacks the response and calls |callback|, which is the original
  // callback that was passed to ExpireHistory().
  void ExpireHistoryCompletionCallback(
      const WebHistoryService::ExpireWebHistoryCallback& callback,
      WebHistoryService::Request* request,
      bool success);

  // Called by |request| when a request to get or set audio history from the
  // server has completed. Unpacks the response and calls |callback|, which is
  // the original callback that was passed to AudioHistory().
  void AudioHistoryCompletionCallback(
    const WebHistoryService::AudioWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success);

  // Called by |request| when a web and app activity query has completed.
  // Unpacks the response and calls |callback|, which is the original callback
  // that was passed to QueryWebAndAppActivity().
  void QueryWebAndAppActivityCompletionCallback(
    const WebHistoryService::QueryWebAndAppActivityCallback& callback,
    WebHistoryService::Request* request,
    bool success);

  // Called by |request| when a query for other forms of browsing history has
  // completed. Unpacks the response and calls |callback|, which is the original
  // callback that was passed to QueryOtherFormsOfBrowsingHistory().
  void QueryOtherFormsOfBrowsingHistoryCompletionCallback(
    const WebHistoryService::QueryWebAndAppActivityCallback& callback,
    WebHistoryService::Request* request,
    bool success);

 private:
  friend class WebHistoryServiceTest;

  // Stores pointer to IdentityManager instance. It must outlive the
  // WebHistoryService and can be null during tests.
  signin::IdentityManager* identity_manager_;

  // Request context getter to use.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Stores the version_info token received from the server in response to
  // a mutation operation (e.g., deleting history). This is used to ensure that
  // subsequent reads see a version of the data that includes the mutation.
  std::string server_version_info_;

  // Pending expiration requests to be canceled if not complete by profile
  // shutdown.
  std::map<Request*, std::unique_ptr<Request>> pending_expire_requests_;

  // Pending requests to be canceled if not complete by profile shutdown.
  std::map<Request*, std::unique_ptr<Request>> pending_audio_history_requests_;

  // Pending web and app activity queries to be canceled if not complete by
  // profile shutdown.
  std::map<Request*, std::unique_ptr<Request>>
      pending_web_and_app_activity_requests_;

  // Pending queries for other forms of browsing history to be canceled if not
  // complete by profile shutdown.
  std::map<Request*, std::unique_ptr<Request>>
      pending_other_forms_of_browsing_history_requests_;

  // Observers.
  base::ObserverList<WebHistoryServiceObserver, true>::Unchecked observer_list_;

  base::WeakPtrFactory<WebHistoryService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebHistoryService);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_H_
