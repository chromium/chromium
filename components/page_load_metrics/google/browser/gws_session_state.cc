// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_session_state.h"

#include "base/metrics/histogram_functions.h"

namespace page_load_metrics {

namespace {
const void* const kGWSSessionStateKey = &kGWSSessionStateKey;
}  // namespace

// static
GWSSessionState* GWSSessionState::GetOrCreateForBrowserContext(
    content::BrowserContext* browser_context) {
  auto* session_state = static_cast<GWSSessionState*>(
      browser_context->GetUserData(kGWSSessionStateKey));
  if (!session_state) {
    session_state = new GWSSessionState(browser_context->IsOffTheRecord());
    browser_context->SetUserData(kGWSSessionStateKey,
                                 base::WrapUnique(session_state));
  }
  CHECK(session_state);
  return session_state;
}

GWSSessionState::GWSSessionState(bool is_off_the_record)
    : is_off_the_record_(is_off_the_record) {}

GWSSessionState::~GWSSessionState() {
  const int page_load_count = base::saturated_cast<int>(page_load_count_);
  base::UmaHistogramCounts1000(
      "PageLoad.Clients.GoogleSearch.GWSSession.PageLoadCount", page_load_count);
  if (is_off_the_record_) {
    base::UmaHistogramCounts1000(
        "PageLoad.Clients.GoogleSearch.GWSSession.PageLoadCount.OffTheRecord",
        page_load_count);
  } else {
    base::UmaHistogramCounts1000(
        "PageLoad.Clients.GoogleSearch.GWSSession.PageLoadCount.Regular",
        page_load_count);
  }
}

void GWSSessionState::SetSignedIn() {
  signed_in_ = true;
}

void GWSSessionState::SetPrewarmed() {
  prewarmed_ = true;
}

void GWSSessionState::IncreasePageLoadCount() {
  page_load_count_++;
}

}  // namespace page_load_metrics
