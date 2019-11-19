// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/service.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Thin C++ wrapper around a service implemented in Java. Intended for use in
// Java UI tests to inject a Java service as a substitute to the native service.
class JavaService : public Service {
 public:
  explicit JavaService(
      const base::android::JavaParamRef<jobject>& java_service);
  ~JavaService() override;

  // Get scripts for a given |url|, which should be a valid URL.
  void GetScriptsForUrl(const GURL& url,
                        const TriggerContext& trigger_context,
                        ResponseCallback callback) override;

  // Get actions.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ResponseCallback callback) override;

  // Get next sequence of actions according to server payloads in previous
  // response.
  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_service_;

  DISALLOW_COPY_AND_ASSIGN(JavaService);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_SERVICE_H_
