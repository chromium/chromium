// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_POSITION_GETTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_POSITION_GETTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill_assistant {

// Worker class to get an element's position in viewport coordinates when it is
// stable and the frame it belongs to has finished its visual update.
class ElementPositionGetter : public WebControllerWorker {
 public:
  // |devtools_client| must be valid for the lifetime of the instance.
  ElementPositionGetter(DevtoolsClient* devtools_client,
                        int max_rounds,
                        base::TimeDelta check_interval,
                        const std::string& optional_node_frame_id);
  ~ElementPositionGetter() override;

  // Callback that receives the position that corresponds to the center
  // of an element.
  //
  // If the operation failed, the status is ELEMENT_UNSTABLE.
  // If the operation succeeded, check the coordinate in the getter.
  using Callback = base::OnceCallback<void(const ClientStatus&)>;

  // The X coordinate of the center of the element, only valid after getting a
  // successful callback.
  int x() { return point_x_; }

  // The Y coordinate of the center of the element, only valid after getting a
  // successful callback.
  int y() { return point_y_; }

  void Start(content::RenderFrameHost* frame_host,
             std::string element_object_id,
             Callback callback);

 private:
  void OnVisualStateUpdatedCallback(bool success);
  void GetAndWaitBoxModelStable();
  void OnGetBoxModelForStableCheck(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::GetBoxModelResult> result);
  void OnScrollIntoView(const DevtoolsClient::ReplyStatus& reply_status,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);
  void RunNextRound();
  void OnResult(int x, int y);
  void OnError(const ClientStatus& status);

  // Time to wait between two box model checks.
  const base::TimeDelta check_interval_;
  // Maximum number of checks to run.
  int max_rounds_;

  raw_ptr<DevtoolsClient> devtools_client_ = nullptr;
  std::string object_id_;
  int remaining_rounds_ = 0;
  Callback callback_;
  bool visual_state_updated_ = false;

  // If |has_point_| is true, |point_x_| and |point_y_| contain the last
  // computed center of the element, in viewport coordinates. Note that
  // negative coordinates are valid, in case the element is above or to the
  // left of the viewport.
  bool has_point_ = false;
  int point_x_ = 0;
  int point_y_ = 0;

  std::string node_frame_id_;

  base::WeakPtrFactory<ElementPositionGetter> weak_ptr_factory_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_POSITION_GETTER_H_
