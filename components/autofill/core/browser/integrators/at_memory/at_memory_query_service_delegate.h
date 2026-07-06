// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace autofill {

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

// A delegate interface for the AtMemoryQueryService to interface with
// browser-level services.
class AtMemoryQueryServiceDelegate {
 public:
  virtual ~AtMemoryQueryServiceDelegate() = default;
  virtual void RetrieveLiveTabContext(
      LiveTabContextQuery query,
      base::OnceCallback<void(LiveTabContextResponse)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_H_
