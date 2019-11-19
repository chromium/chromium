// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_DETECTOR_H_
#define COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_DETECTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/captive_portal/captive_portal_export.h"
#include "components/captive_portal/captive_portal_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if defined(OS_CHROMEOS)
#include "base/memory/weak_ptr.h"
#endif

class GURL;

namespace captive_portal {

class CAPTIVE_PORTAL_EXPORT CaptivePortalDetector {
 public:
  struct Results {
    captive_portal::CaptivePortalResult result =
        captive_portal::RESULT_NO_RESPONSE;
    int response_code = 0;
    base::TimeDelta retry_after_delta;
    GURL landing_url;
  };

  typedef base::OnceCallback<void(const Results& results)> DetectionCallback;

  // The test URL.  When connected to the Internet, it should return a
  // blank page with a 204 status code.  When behind a captive portal,
  // requests for this URL should get an HTTP redirect or a login
  // page.  When neither is true, no server should respond to requests
  // for this URL.
  static const char kDefaultURL[];

  explicit CaptivePortalDetector(
      network::mojom::URLLoaderFactory* loader_factory);
  ~CaptivePortalDetector();

  // Triggers a check for a captive portal. After completion, runs the
  // |callback|.
  void DetectCaptivePortal(
      const GURL& url,
      DetectionCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Cancels captive portal check.
  void Cancel();

 private:
  friend class CaptivePortalDetectorTestBase;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  void OnSimpleLoaderCompleteInternal(int net_error,
                                      int response_code,
                                      const GURL& url,
                                      net::HttpResponseHeaders* headers);

  // Fills a Results struct based on the response from SimpleURLLoader. If the
  // response is a 503 with a Retry-After header, |retry_after| field
  // of |results| is populated accordingly.  Otherwise, it's set to
  // base::TimeDelta().
  void GetCaptivePortalResultFromResponse(int net_error,
                                          int response_code,
                                          const GURL& url,
                                          net::HttpResponseHeaders* headers,
                                          Results* results) const;

  // Starts portal detection probe after GetProbeUrl finishes running.
  void StartProbe(const net::NetworkTrafficAnnotationTag& traffic_annotation,
                  const GURL& url);

  // Returns the current time. Used only when determining time until a
  // Retry-After date.
  base::Time GetCurrentTime() const;

  // Returns true if a captive portal check is currently running.
  bool FetchingURL() const;

  // Sets current test time. Used by unit tests.
  void set_time_for_testing(const base::Time& time) {
    time_for_testing_ = time;
  }

  // Advances current test time. Used by unit tests.
  void advance_time_for_testing(const base::TimeDelta& delta) {
    time_for_testing_ += delta;
  }

  DetectionCallback detection_callback_;

  network::mojom::URLLoaderFactory* loader_factory_ = nullptr;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // Test time used by unit tests.
  base::Time time_for_testing_;

  // Probe URL accessed by tests.
  GURL probe_url_;

  SEQUENCE_CHECKER(sequence_checker_);

#if defined(OS_CHROMEOS)
  base::WeakPtrFactory<CaptivePortalDetector> weak_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDetector);
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_DETECTOR_H_
