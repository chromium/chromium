// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOCUMENT_OPERATION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOCUMENT_OPERATION_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {

// Waits for a minimal state of the document or times out if the state is not
// reached in time.
class WaitForDocumentOperation {
 public:
  using Callback = base::OnceCallback<
      void(const ClientStatus&, DocumentReadyState, base::TimeDelta)>;

  // |script_executor_delegate| must outlive this instance.
  WaitForDocumentOperation(ScriptExecutorDelegate* script_executor_delegate,
                           base::TimeDelta max_wait_time,
                           DocumentReadyState min_ready_state,
                           const ElementFinderResult& optional_frame_element,
                           Callback callback);
  ~WaitForDocumentOperation();

  WaitForDocumentOperation(const WaitForDocumentOperation&) = delete;
  WaitForDocumentOperation& operator=(const WaitForDocumentOperation&) = delete;

  void Run();

 private:
  void OnTimeout(base::TimeTicks wait_time_start);
  void OnWaitForState(const ClientStatus& status,
                      DocumentReadyState current_state,
                      base::TimeDelta wait_time);

  raw_ptr<ScriptExecutorDelegate> script_executor_delegate_;
  base::TimeDelta max_wait_time_;
  DocumentReadyState min_ready_state_;
  const ElementFinderResult& optional_frame_element_;
  Callback callback_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<WaitForDocumentOperation> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOCUMENT_OPERATION_H_
