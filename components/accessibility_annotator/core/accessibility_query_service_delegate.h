// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace accessibility_annotator {

// Represents a natural language query for live tab context.
struct LiveTabContextQuery {
  LiveTabContextQuery();
  LiveTabContextQuery(const LiveTabContextQuery&);
  LiveTabContextQuery& operator=(const LiveTabContextQuery&);
  ~LiveTabContextQuery();

  std::u16string query;
};

// Represents the response containing relevant results from live tabs.
struct LiveTabContextResponse {
  LiveTabContextResponse();
  LiveTabContextResponse(const LiveTabContextResponse&);
  LiveTabContextResponse& operator=(const LiveTabContextResponse&);
  ~LiveTabContextResponse();

  std::vector<std::u16string> results;
};

// A delegate interface for the AccessibilityQueryService to interface with
// browser-level services.
class AccessibilityQueryServiceDelegate {
 public:
  virtual ~AccessibilityQueryServiceDelegate() = default;
  virtual void RetrieveLiveTabContext(
      LiveTabContextQuery query,
      base::OnceCallback<void(LiveTabContextResponse)> callback) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_DELEGATE_H_
