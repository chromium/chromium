// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_GEN204_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_GEN204_CONTROLLER_H_

#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "services/network/public/cpp/simple_url_loader.h"

class Profile;

namespace lens {

// Sends gen204 pings for the Lens Overlay.
class LensOverlayGen204Controller {
 public:
  LensOverlayGen204Controller(
      lens::LensOverlayInvocationSource invocation_source,
      Profile* profile);
  ~LensOverlayGen204Controller();

  // Sets the gen204 id, to be attached to subsequent gen204 requests.
  void SetGen204Id(uint64_t gen204_id);

  // Sends a Lens objects request latency gen204 request.
  void SendLatencyGen204IfEnabled(int64_t latency_ms, bool is_translate_query);

  // Sends a task completion gen204 request. The analytics id is the
  // latest Lens request analytics id from the query controller.
  // The user action is the action that triggered the task completion
  // event.
  void SendTaskCompletionGen204IfEnabled(std::string encoded_analytics_id,
                                         lens::mojom::UserAction user_action);

  // Sends any final gen204 requests and marks the end of the query flow.
  // The analytics id is the latest Lens request analytics id from the
  // query controller.
  void OnQueryFlowEnd(std::string encoded_analytics_id);

 private:
  // Callback for the latency gen204 request.
  void OnLatencyGen204Response(std::unique_ptr<std::string> response_body);

  // Callback for the task completion gen204 request.
  void OnTaskCompletionGen204Response(
      std::unique_ptr<std::string> response_body);

  // Loader used for latency gen204 requests.
  std::unique_ptr<network::SimpleURLLoader> latency_gen204_loader_;

  // Loader used for task completion gen204 requests.
  std::unique_ptr<network::SimpleURLLoader> task_completion_gen204_loader_;

  // The invocation source that triggered the query flow.
  lens::LensOverlayInvocationSource invocation_source_;

  // The current gen204 id for logging, set by the query controller.
  uint64_t gen204_id_;

  // The profile used to make requests.
  const raw_ptr<Profile> profile_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_GEN204_CONTROLLER_H_
