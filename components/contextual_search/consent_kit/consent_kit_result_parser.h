// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_RESULT_PARSER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_RESULT_PARSER_H_

#include "components/contextual_search/consent_kit/proto/iframe_interface.pb.h"

namespace drive {

// Returns true if `result` belongs to `expected_flow_id`. Callers supply the
// id, mirroring ConsentKitUrlBuilder::SetFlowId() on the request side.
bool IsExpectedConsentFlow(const identity_consent::PrivacyFlowResult& result,
                           int expected_flow_id);

// Returns true if `result` grants or re-affirms Workspace search consent.
bool HasGrantedDriveConsent(const identity_consent::PrivacyFlowResult& result);

}  // namespace drive

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONSENT_KIT_CONSENT_KIT_RESULT_PARSER_H_
