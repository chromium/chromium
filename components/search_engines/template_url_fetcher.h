// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class TemplateURL;
class TemplateURLService;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace url {
class Origin;
}

// TemplateURLFetcher is responsible for downloading OpenSearch description
// documents, creating a TemplateURL from the OSDD, and adding the TemplateURL
// to the TemplateURLService. Downloading is done in the background.
//
class TemplateURLFetcher : public KeyedService {
 public:
  // Creates a TemplateURLFetcher.
  explicit TemplateURLFetcher(TemplateURLService* template_url_service);

  TemplateURLFetcher(const TemplateURLFetcher&) = delete;
  TemplateURLFetcher& operator=(const TemplateURLFetcher&) = delete;

  ~TemplateURLFetcher() override;

  // If TemplateURLFetcher is not already downloading the OSDD for osdd_url,
  // it is downloaded. If successful and the result can be parsed, a TemplateURL
  // is added to the TemplateURLService.
  //
  // |keyword| must be non-empty. If there's already a non-replaceable
  // TemplateURL in the model for |keyword|, or we're already downloading an
  // OSDD for this keyword, no download is started.
  //
  void ScheduleDownload(const std::u16string& keyword,
                        const GURL& osdd_url,
                        const GURL& favicon_url,
                        const url::Origin& initiator,
                        network::mojom::URLLoaderFactory* url_loader_factory,
                        int render_frame_id,
                        int32_t request_id);

  // The current number of outstanding requests.
  int requests_count() const { return requests_.size(); }

 protected:
  // A RequestDelegate is created to download each OSDD. When done downloading
  // RequestCompleted is invoked back on the TemplateURLFetcher.
  class RequestDelegate;

  // Invoked from the RequestDelegate when done downloading. Virtual for tests.
  virtual void RequestCompleted(RequestDelegate* request);

 private:
  friend class RequestDelegate;

  raw_ptr<TemplateURLService> template_url_service_;

  // In progress requests.
  std::vector<std::unique_ptr<RequestDelegate>> requests_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
