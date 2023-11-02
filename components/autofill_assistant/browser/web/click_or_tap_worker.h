// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CLICK_OR_TAP_WORKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CLICK_OR_TAP_WORKER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/action_strategy.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_input.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/web/element_position_getter.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace autofill_assistant {
class ElementFinderResult;

// Worker class for sending click or tap events.
class ClickOrTapWorker : public WebControllerWorker {
 public:
  // |devtools_client| must be valid for the lifetime of the instance.
  ClickOrTapWorker(DevtoolsClient* devtools_client);
  ~ClickOrTapWorker() override;

  // Callback called when the worker is done.
  using Callback = base::OnceCallback<void(const ClientStatus&)>;

  // Send a click or tap event to the the |elemsent|.
  void Start(const ElementFinderResult& element,
             ClickType click_type,
             Callback callback);

 private:
  void OnGetCoordinates(const ClientStatus& status);

  void OnDispatchPressMouseEvent(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);
  void OnDispatchReleaseMouseEvent(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchMouseEventResult> result);

  void OnDispatchTouchEventStart(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);
  void OnDispatchTouchEventEnd(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<input::DispatchTouchEventResult> result);

  const raw_ptr<DevtoolsClient> devtools_client_;
  Callback callback_;
  ClickType click_type_;
  std::string node_frame_id_;

  std::unique_ptr<ElementPositionGetter> element_position_getter_;

  base::WeakPtrFactory<ClickOrTapWorker> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_CLICK_OR_TAP_WORKER_H_
