// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_session_state.h"

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
    session_state = new GWSSessionState();
    browser_context->SetUserData(kGWSSessionStateKey,
                                 base::WrapUnique(session_state));
  }
  CHECK(session_state);
  return session_state;
}

GWSSessionState::GWSSessionState() = default;
GWSSessionState::~GWSSessionState() = default;

void GWSSessionState::SetSignedIn() {
  signed_in_ = true;
}

void GWSSessionState::SetPrewarmed() {
  prewarmed_ = true;
}

}  // namespace page_load_metrics
