// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TESTING_UTILS_H_
#define COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TESTING_UTILS_H_

#include <string>

#include "base/macros.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "services/network/test/test_url_loader_factory.h"

namespace base {
class Time;
}

namespace captive_portal {

class CaptivePortalDetectorTestBase {
 public:
  CaptivePortalDetectorTestBase();
  virtual ~CaptivePortalDetectorTestBase();

  // Sets test time for captive portal detector.
  void SetTime(const base::Time& time);

  // Advances test time for captive portal detector.
  void AdvanceTime(const base::TimeDelta& time_delta);

  bool FetchingURL();

  // Sets URL fetcher state and notifies portal detector.
  void CompleteURLFetch(int net_error,
                        int status_code,
                        const char* response_headers);

  void set_detector(CaptivePortalDetector* detector) { detector_ = detector; }

  network::TestURLLoaderFactory* test_loader_factory() {
    return &test_loader_factory_;
  }

  CaptivePortalDetector* detector() { return detector_; }

  const GURL& get_probe_url() { return detector_->probe_url_; }

 protected:
  CaptivePortalDetector* detector_;
  network::TestURLLoaderFactory test_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDetectorTestBase);
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CAPTIVE_PORTAL_TESTING_UTILS_H_
