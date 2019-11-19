// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_

#include <stddef.h>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/variations/variations_http_header_provider.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class PrefService;

namespace autofill {

class AutofillDriver;
class FormStructure;
class LogManager;

const size_t kMaxAPIQueryGetSize = 10240;  // 10 KiB

// A helper to make sure that tests which modify the set of active autofill
// experiments do not interfere with one another.
struct ScopedActiveAutofillExperiments {
  ScopedActiveAutofillExperiments();
  ~ScopedActiveAutofillExperiments();
};

// Handles getting and updating Autofill heuristics.
class AutofillDownloadManager {
 public:
  enum RequestType { REQUEST_QUERY, REQUEST_UPLOAD, };

  // An interface used to notify clients of AutofillDownloadManager.
  class Observer {
   public:
    // Called when field type predictions are successfully received from the
    // server. |response| contains the server response for the forms
    // represented by |form_signatures|.
    virtual void OnLoadedServerPredictions(
        std::string response,
        const std::vector<std::string>& form_signatures) = 0;

    // These notifications are used to help with testing.
    // Called when heuristic either successfully considered for upload and
    // not send or uploaded.
    virtual void OnUploadedPossibleFieldTypes() {}
    // Called when there was an error during the request.
    // |form_signature| - the signature of the requesting form.
    // |request_type| - type of request that failed.
    // |http_error| - HTTP error code.
    virtual void OnServerRequestError(const std::string& form_signature,
                                      RequestType request_type,
                                      int http_error) {}

   protected:
    virtual ~Observer() {}
  };

  // |driver| must outlive this instance.
  // |observer| - observer to notify on successful completion or error.
  // Uses an API callback function that gives an empty string.
  AutofillDownloadManager(AutofillDriver* driver, Observer* observer);
  // |driver| must outlive this instance.
  // |observer| - observer to notify on successful completion or error.
  // |api_key| - API key to add to API request query parameters. Will only take
  //   effect if using API.
  AutofillDownloadManager(AutofillDriver* driver,
                          Observer* observer,
                          const std::string& api_key,
                          LogManager* log_manager);
  virtual ~AutofillDownloadManager();

  // Starts a query request to Autofill servers. The observer is called with the
  // list of the fields of all requested forms.
  // |forms| - array of forms aggregated in this request.
  virtual bool StartQueryRequest(const std::vector<FormStructure*>& forms);

  // Starts an upload request for the given |form|.
  // |available_field_types| should contain the types for which we have data
  // stored on the local client.
  // |login_form_signature| may be empty. It is non-empty when the user fills
  // and submits a login form using a generated password. In this case,
  // |login_form_signature| should be set to the submitted form's signature.
  // Note that in this case, |form.FormSignature()| gives the signature for the
  // registration form on which the password was generated, rather than the
  // submitted form's signature.
  // |observed_submission| indicates whether the upload request is the result of
  // an observed submission event.
  virtual bool StartUploadRequest(
      const FormStructure& form,
      bool form_was_autofilled,
      const ServerFieldTypeSet& available_field_types,
      const std::string& login_form_signature,
      bool observed_submission,
      PrefService* pref_service);

  // Returns true if the autofill server communication is enabled.
  bool IsEnabled() const { return autofill_server_url_.is_valid(); }

  // Reset the upload history. This reduced space history prevents the autofill
  // download manager from uploading a multiple votes for a given form/event
  // pair.
  static void ClearUploadHistory(PrefService* pref_service);

 protected:
  // Gets the length of the payload from request data. Used to simulate
  // different payload sizes when testing without the need for data. Do not use
  // this when the length is needed to read/write a buffer.
  virtual size_t GetPayloadLength(base::StringPiece payload) const;

 private:
  friend class AutofillDownloadManagerTest;
  friend struct ScopedActiveAutofillExperiments;
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, QueryAndUploadTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, BackoffLogic_Upload);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, BackoffLogic_Query);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, RetryLimit_Upload);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, RetryLimit_Query);

  struct FormRequestData;
  typedef std::list<std::pair<std::string, std::string> > QueryRequestCache;

  // Returns the URL and request method to use when issuing the request
  // described by |request_data|. If the returned method is GET, the URL
  // fully encompasses the request, do not include request_data.payload when
  // transmitting the request.
  std::tuple<GURL, std::string> GetRequestURLAndMethod(
      const FormRequestData& request_data) const;

  // Same as GetRequestURLAndMethod, but for the API.
  std::tuple<GURL, std::string> GetRequestURLAndMethodForApi(
      const FormRequestData& request_data) const;

  // Initiates request to Autofill servers to download/upload type predictions.
  // |request_data| - form signature hash(es), request payload data and request
  //   type (query or upload).
  // Note: |request_data| takes ownership of request_data, call with std::move.
  bool StartRequest(FormRequestData request_data);

  // Each request is page visited. We store last |max_form_cache_size|
  // request, to avoid going over the wire. Set to 16 in constructor. Warning:
  // the search is linear (newest first), so do not make the constant very big.
  void set_max_form_cache_size(size_t max_form_cache_size) {
    max_form_cache_size_ = max_form_cache_size;
  }

  // Caches query request. |forms_in_query| is a vector of form signatures in
  // the query. |query_data| is the successful data returned over the wire.
  void CacheQueryRequest(const std::vector<std::string>& forms_in_query,
                         const std::string& query_data);
  // Returns true if query is in the cache, while filling |query_data|, false
  // otherwise. |forms_in_query| is a vector of form signatures in the query.
  bool CheckCacheForQueryRequest(const std::vector<std::string>& forms_in_query,
                                 std::string* query_data) const;
  // Concatenates |forms_in_query| into one signature.
  std::string GetCombinedSignature(
      const std::vector<std::string>& forms_in_query) const;

  // Returns the maximum number of attempts for a given autofill server request.
  static int GetMaxServerAttempts();

  void OnSimpleLoaderComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      FormRequestData request_data,
      base::TimeTicks request_start,
      std::unique_ptr<std::string> response_body);

  static void InitActiveExperiments();
  static void ResetActiveExperiments();

  // The AutofillDriver that this instance will use. Must not be null, and must
  // outlive this instance.
  AutofillDriver* const driver_;  // WEAK

  // The observer to notify when server predictions are successfully received.
  // Must not be null.
  AutofillDownloadManager::Observer* const observer_;  // WEAK

  // Callback function to retrieve API key.
  const std::string api_key_;

  // Access to leave log messages for chrome://autofill-internals, may be null.
  LogManager* const log_manager_;  // WEAK

  // The autofill server URL root: scheme://host[:port]/path excluding the
  // final path component for the request and the query params.
  const GURL autofill_server_url_;

  // The period after which the tracked set of uploads to throttle is reset.
  const base::TimeDelta throttle_reset_period_;

  // The set of active autofill server experiments.
  static std::vector<variations::VariationID>* active_experiments_;

  // Loaders used for the processing the requests. Invalidated after completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  // Cached QUERY requests.
  QueryRequestCache cached_forms_;
  size_t max_form_cache_size_;

  // Used for exponential backoff of requests.
  net::BackoffEntry loader_backoff_;

  base::WeakPtrFactory<AutofillDownloadManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_
