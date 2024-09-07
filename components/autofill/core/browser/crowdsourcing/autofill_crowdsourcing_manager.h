// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_H_

#include <stddef.h>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/version_info/channel.h"
#include "net/base/backoff_entry.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class PrefService;

namespace autofill {

class AutofillClient;
class LogManager;

inline constexpr size_t kMaxQueryGetSize = 10240;  // 10 KiB

// A helper to make sure that tests which modify the set of active autofill
// experiments do not interfere with one another.
struct ScopedActiveAutofillExperiments {
  ScopedActiveAutofillExperiments();
  ~ScopedActiveAutofillExperiments();
};

// Obtains Autofill server predictions and upload votes for generating them.
class AutofillCrowdsourcingManager {
 public:
  // Names of UMA metrics recorded in this class.
  static constexpr char kUmaApiUrlIsTooLong[] =
      "Autofill.Query.ApiUrlIsTooLong";
  static constexpr char kUmaGetUrlLength[] = "Autofill.Query.GetUrlLength";
  static constexpr char kUmaMethod[] = "Autofill.Query.Method";
  static constexpr char kUmaWasInCache[] = "Autofill.Query.WasInCache";

  // `client` owns (and hence survives) this AutofillCrowdsourcingManager.
  // `channel` determines the value for the the Google-API-key HTTP header and
  // whether raw metadata uploading is enabled.
  AutofillCrowdsourcingManager(AutofillClient* client,
                          version_info::Channel channel,
                          LogManager* log_manager);

  virtual ~AutofillCrowdsourcingManager();

  struct QueryResponse {
    QueryResponse(std::string response,
                  std::vector<FormSignature> queried_form_signatures);
    QueryResponse(QueryResponse&&);
    QueryResponse& operator=(QueryResponse&&);
    ~QueryResponse();

    std::string response;
    std::vector<FormSignature> queried_form_signatures;
  };

  // Starts a query request to Autofill servers for `forms`. It always calls
  // `callback`: with the QueryResponse if the query is successful and with
  // std::nullopt if it the query wasn't made or was unsuccessful.
  //
  // Returns true if a query is made.
  // TODO: crbug.com/40100455 - Make the return type `void`.
  virtual bool StartQueryRequest(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
      std::optional<net::IsolationInfo> isolation_info,
      base::OnceCallback<void(std::optional<QueryResponse>)> callback);

  // Starts an upload request for `upload_contents`. If `upload_contents` has
  // more than one element, then `upload_contents[0]` is expected to correspond
  // to the browser form and `upload_contents[i]` with `i>0` are expected to
  // correspond to the renderer forms that constitute the browser form.
  // See `autofill::FormForest` for more information on browser vs renderer
  // forms.
  virtual bool StartUploadRequest(
      std::vector<AutofillUploadContents> upload_contents,
      mojom::SubmissionSource form_submission_source,
      bool is_password_manager_upload);

  // Returns true if the autofill server communication is enabled.
  bool IsEnabled() const;

  // Reset the upload history. This reduced space history prevents the autofill
  // download manager from uploading a multiple votes for a given form/event
  // pair.
  static void ClearUploadHistory(PrefService* pref_service);

  // Returns the maximum number of attempts for a given autofill server request.
  static int GetMaxServerAttempts();

 protected:
  AutofillCrowdsourcingManager(AutofillClient* client,
                               const std::string& api_key,
                               LogManager* log_manager);

  // Gets the length of the payload from request data. Used to simulate
  // different payload sizes when testing without the need for data. Do not use
  // this when the length is needed to read/write a buffer.
  virtual size_t GetPayloadLength(std::string_view payload) const;

 private:
  friend class AutofillCrowdsourcingManagerTestApi;
  friend struct ScopedActiveAutofillExperiments;

  struct FormRequestData;
  using QueryRequestCache =
      std::list<std::pair<std::vector<FormSignature>, std::string>>;

  // Returns the URL and request method to use when issuing the request
  // described by |request_data|. If the returned method is GET, the URL
  // fully encompasses the request, do not include request_data.payload when
  // transmitting the request.
  std::tuple<GURL, std::string> GetRequestURLAndMethod(
      const FormRequestData& request_data) const;

  // Initiates request to Autofill servers to download/upload type predictions.
  // |request_data| - form signature hash(es), request payload data and request
  //   type (query or upload).
  // Note: |request_data| takes ownership of request_data, call with std::move.
  bool StartRequest(FormRequestData request_data);

  // Caches query request. |forms_in_query| is a vector of form signatures in
  // the query. |query_data| is the successful data returned over the wire.
  void CacheQueryRequest(const std::vector<FormSignature>& forms_in_query,
                         const std::string& query_data);
  // Returns true if query is in the cache, while filling |query_data|, false
  // otherwise. |forms_in_query| is a vector of form signatures in the query.
  bool CheckCacheForQueryRequest(
      const std::vector<FormSignature>& forms_in_query,
      std::string* query_data) const;
  // Concatenates |forms_in_query| into one signature.
  std::string GetCombinedSignature(
      const std::vector<std::string>& forms_in_query) const;

  void OnSimpleLoaderComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      FormRequestData request_data,
      base::TimeTicks request_start,
      std::unique_ptr<std::string> response_body);

  // The AutofillClient that this instance will use. Must not be null, and must
  // outlive this instance.
  const raw_ptr<AutofillClient> client_;

  // Callback function to retrieve API key.
  const std::string api_key_;

  // Access to leave log messages for chrome://autofill-internals, may be null.
  const raw_ptr<LogManager> log_manager_;

  // The autofill server URL root: scheme://host[:port]/path excluding the
  // final path component for the request and the query params.
  const GURL autofill_server_url_;

  // The period after which the tracked set of uploads to throttle is reset.
  const base::TimeDelta throttle_reset_period_;

  // Loaders used for the processing the requests. Invalidated after completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  // Cached QUERY requests.
  QueryRequestCache cached_forms_;
  size_t max_form_cache_size_;

  // Used for exponential backoff of requests.
  net::BackoffEntry loader_backoff_;

  base::WeakPtrFactory<AutofillCrowdsourcingManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_H_
