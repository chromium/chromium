// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/clamped_math.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_SESSION_STATE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_SESSION_STATE_H_

namespace page_load_metrics {

class GWSSessionState : public base::SupportsUserData::Data {
 public:
  static GWSSessionState* GetOrCreateForBrowserContext(
      content::BrowserContext* browser_context);

  explicit GWSSessionState(bool is_off_the_record);
  GWSSessionState(const GWSSessionState&) = delete;
  GWSSessionState& operator=(const GWSSessionState&) = delete;
  ~GWSSessionState() override;

  void SetSignedIn();
  void SetPrewarmed();

  bool IsSignedIn() const { return signed_in_; }
  bool IsPrewarmed() const { return prewarmed_; }

  void IncreasePageLoadCount();

 private:
  const bool is_off_the_record_;
  bool signed_in_ = false;
  bool prewarmed_ = false;
  base::ClampedNumeric<size_t> page_load_count_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_SESSION_STATE_H_
