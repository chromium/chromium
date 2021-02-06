// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace base {
class Value;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace cloud_print {

// Factory for creating CloudPrintURLFetchers.
class CloudPrintURLFetcher;
class CloudPrintURLFetcherFactory {
 public:
  virtual CloudPrintURLFetcher* CreateCloudPrintURLFetcher() = 0;
  virtual ~CloudPrintURLFetcherFactory();
};

// A wrapper around URLFetcher for CloudPrint. URLFetcher applies retry logic
// only on HTTP response codes >= 500. In the cloud print case, we want to
// retry on all network errors. In addition, we want to treat non-JSON responses
// (for all CloudPrint APIs that expect JSON responses) as errors and they
// must also be retried.
class CloudPrintURLFetcher
    : public base::RefCountedThreadSafe<CloudPrintURLFetcher>,
      public net::URLFetcherDelegate {
 public:
  enum ResponseAction {
    CONTINUE_PROCESSING,
    STOP_PROCESSING,
    RETRY_REQUEST,
  };

  enum RequestType {
    REQUEST_AUTH_CODE,
    REQUEST_REGISTER,
    REQUEST_UNREGISTER,
    REQUEST_UPDATE_PRINTER,
    REQUEST_UPDATE_JOB,
    REQUEST_USER_MESSAGE,
    REQUEST_TICKET,
    REQUEST_DATA,
    REQUEST_JOB_FETCH,
    REQUEST_MAX,
  };

  class Delegate {
   public:
    // Override this to handle the raw response as it is available. No response
    // error checking is done before this method is called. If the delegate
    // returns CONTINUE_PROCESSING, we will then check for network
    // errors. Most implementations will not override this.
    virtual ResponseAction HandleRawResponse(const net::URLFetcher* source,
                                             const GURL& url,
                                             net::Error error,
                                             int response_code,
                                             const std::string& data);

    // This will be invoked only if HandleRawResponse returns
    // CONTINUE_PROCESSING AND if there are no network errors and the HTTP
    // response code is 200. The delegate implementation returns
    // CONTINUE_PROCESSING if it does not want to handle the raw data itself.
    // Handling the raw data is needed when the expected response is NOT JSON
    // (like in the case of a print ticket response or a print job download
    // response).
    virtual ResponseAction HandleRawData(const net::URLFetcher* source,
                                         const GURL& url,
                                         const std::string& data);

    // This will be invoked only if HandleRawResponse and HandleRawData return
    // CONTINUE_PROCESSING AND if the response contains a valid JSON dictionary.
    // |succeeded| is the value of the "success" field in the response JSON.
    virtual ResponseAction HandleJSONData(const net::URLFetcher* source,
                                          const GURL& url,
                                          const base::Value& json_data,
                                          bool succeeded);

    // Invoked when the retry limit for this request has been reached (if there
    // was a retry limit - a limit of -1 implies no limit).
    virtual void OnRequestGiveUp() { }

    // Invoked when the request returns a 403 error (applicable only when
    // HandleRawResponse returns CONTINUE_PROCESSING).
    // Returning RETRY_REQUEST will retry current request. (auth information
    // may have been updated and new info is available through the
    // Authenticator interface).
    // Returning CONTINUE_PROCESSING will treat auth error as a network error.
    virtual ResponseAction OnRequestAuthError() = 0;

    // Authentication information may change between retries.
    // CloudPrintURLFetcher will request auth info before sending any request.
    virtual std::string GetAuthHeaderValue() = 0;

   protected:
    virtual ~Delegate() {}
  };

  static CloudPrintURLFetcher* Create(
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);
  static void set_test_factory(CloudPrintURLFetcherFactory* factory);

  bool IsSameRequest(const net::URLFetcher* source);

  void StartGetRequest(RequestType type,
                       const GURL& url,
                       Delegate* delegate,
                       int max_retries);
  void StartGetRequestWithAcceptHeader(RequestType type,
                                       const GURL& url,
                                       Delegate* delegate,
                                       int max_retries,
                                       const std::string& accept_header);
  void StartPostRequest(RequestType type,
                        const GURL& url,
                        Delegate* delegate,
                        int max_retries,
                        const std::string& post_data_mime_type,
                        const std::string& post_data);

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 protected:
  CloudPrintURLFetcher(const net::PartialNetworkTrafficAnnotationTag&
                           partial_traffic_annotation);
  friend class base::RefCountedThreadSafe<CloudPrintURLFetcher>;
  ~CloudPrintURLFetcher() override;

  // Virtual for testing.
  virtual net::URLRequestContextGetter* GetRequestContextGetter();

 private:
  void StartRequestHelper(RequestType type,
                          const GURL& url,
                          net::URLFetcher::RequestType request_type,
                          Delegate* delegate,
                          int max_retries,
                          const std::string& post_data_mime_type,
                          const std::string& post_data,
                          const std::string& additional_accept_header);
  void SetupRequestHeaders();

  std::unique_ptr<net::URLFetcher> request_;
  Delegate* delegate_;
  int num_retries_;
  std::string additional_accept_header_;
  std::string post_data_mime_type_;
  std::string post_data_;

  RequestType type_;
  base::Time start_time_;
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
