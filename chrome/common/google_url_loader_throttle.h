// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_

#include "chrome/common/renderer_configuration.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// This class changes requests for Google-specific features (e.g. adding &
// removing Varitaions headers, Safe Search & Restricted YouTube & restricting
// consumer accounts through group policy.
class GoogleURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<GoogleURLLoaderThrottle> {
 public:
  GoogleURLLoaderThrottle(bool is_off_the_record,
                          chrome::mojom::DynamicParams dynamic_params);
  ~GoogleURLLoaderThrottle() override;

 private:
  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* to_be_removed_headers,
                           net::HttpRequestHeaders* modified_headers) override;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
#endif

  bool is_off_the_record_;
  const chrome::mojom::DynamicParams dynamic_params_;
};

#endif  // CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
