// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_WIPEOUT_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_WIPEOUT_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace cloud_print {

// CloudPrintWipeout unregisters list of printers from the cloudprint service.
class CloudPrintWipeout : public CloudPrintURLFetcher::Delegate {
 public:
  class Client {
   public:
    virtual void OnUnregisterPrintersComplete() = 0;
   protected:
     virtual ~Client() {}
  };

  CloudPrintWipeout(Client* client,
                    const GURL& cloud_print_server_url,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation);
  ~CloudPrintWipeout() override;

  void UnregisterPrinters(const std::string& auth_token,
                          const std::list<std::string>& printer_ids);

  // CloudPrintURLFetcher::Delegate implementation.
  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;
  void OnRequestGiveUp() override;
  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override;
  std::string GetAuthHeader() override;

 private:
  void UnregisterNextPrinter();

  // CloudPrintWipeout client.
  Client* client_;
  // Cloud Print server url.
  GURL cloud_print_server_url_;
  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;
  // Auth token.
  std::string auth_token_;
  // List of printer to unregister
  std::list<std::string> printer_ids_;
  // Partial network traffic annotation for network requests.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintWipeout);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_WIPEOUT_H_

