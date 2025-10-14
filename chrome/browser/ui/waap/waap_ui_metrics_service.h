// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class WaapUIMetricsServiceFactory;

// `WaapUIMetricsService` is responsible for receiving UI metrics from WaaP UI
// elements, either renderers or browsers.
//
// It is scoped to the lifetime of a Profile, and is expected to be created in
// all kinds of profiles.
class WaapUIMetricsService : public KeyedService {
 public:
  explicit WaapUIMetricsService(base::PassKey<WaapUIMetricsServiceFactory>);

  // Disallow copy and assign.
  WaapUIMetricsService(const WaapUIMetricsService&) = delete;
  WaapUIMetricsService& operator=(const WaapUIMetricsService&) = delete;

  ~WaapUIMetricsService() override;

  // Convenient method to get an instance for the given `profile`.
  // May return nullptr.
  static WaapUIMetricsService* Get(Profile* profile);

  // Called whenever the WaaP UI has its first paint finished.
  void OnFirstPaint(base::TimeTicks time);

  // Called whenever the WaaP UI has its first contentful paint finished.
  void OnFirstContentfulPaint(base::TimeTicks time);
};

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UI_METRICS_SERVICE_H_
