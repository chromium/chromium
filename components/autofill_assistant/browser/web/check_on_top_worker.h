// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CHECK_ON_TOP_WORKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CHECK_ON_TOP_WORKER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace autofill_assistant {
class ElementFinderResult;

// Worker class to check whether an element is on top, in all frames.
class CheckOnTopWorker : public WebControllerWorker {
 public:
  // |devtools_client| must be valid for the lifetime of the instance.
  CheckOnTopWorker(DevtoolsClient* devtools_client);
  ~CheckOnTopWorker() override;

  // Callback called when the worker is done.
  using Callback = base::OnceCallback<void(const ClientStatus&)>;

  // Have the worker check |element| and report the result to |callback|.
  void Start(const ElementFinderResult& element, Callback callback);

 private:
  void CallFunctionOn(const std::string& function,
                      const std::string& frame_id,
                      const std::string& object_id);
  void OnReply(const DevtoolsClient::ReplyStatus& reply_status,
               std::unique_ptr<runtime::CallFunctionOnResult> result);

  const raw_ptr<DevtoolsClient> devtools_client_;
  Callback callback_;

  // The number of successful results that are still expected before the check
  // can be reported as successful. Note that an unsuccessful result is reported
  // right away.
  size_t pending_result_count_;

  base::WeakPtrFactory<CheckOnTopWorker> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CHECK_ON_TOP_WORKER_H_
