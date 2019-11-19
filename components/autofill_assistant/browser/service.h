// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "url/gurl.h"

namespace autofill_assistant {
class TriggerContext;

// Base interface for an autofill assistant service. Services provide methods to
// asynchronously query scripts for a particular URL, as well as actions for a
// particular script.
class Service {
 public:
  virtual ~Service() = default;

  using ResponseCallback =
      base::OnceCallback<void(bool result, const std::string&)>;
  // Get scripts for a given |url|, which should be a valid URL.
  virtual void GetScriptsForUrl(const GURL& url,
                                const TriggerContext& trigger_context,
                                ResponseCallback callback) = 0;

  // Get actions.
  virtual void GetActions(const std::string& script_path,
                          const GURL& url,
                          const TriggerContext& trigger_context,
                          const std::string& global_payload,
                          const std::string& script_payload,
                          ResponseCallback callback) = 0;

  // Get next sequence of actions according to server payloads in previous
  // response.
  virtual void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) = 0;

 protected:
  Service() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
