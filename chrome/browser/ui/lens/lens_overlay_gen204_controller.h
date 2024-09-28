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
  LensOverlayGen204Controller();
  ~LensOverlayGen204Controller();

  // Sets the state of the controller. Should be called once
  // per query flow, at the start.
  void OnQueryFlowStart(lens::LensOverlayInvocationSource invocation_source,
                        Profile* profile,
                        uint64_t gen204_id);

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
  // Issues the gen204 network request and adds a loader to gen204_loaders_.
  void IssueGen204NetworkRequest(GURL url);

  // Handles the gen204 network response and removes the source from
  // gen204_loaders_.
  void OnGen204NetworkResponse(const network::SimpleURLLoader* source,
                               std::unique_ptr<std::string> response_body);

  // The invocation source that triggered the query flow.
  lens::LensOverlayInvocationSource invocation_source_;

  // The profile used to make requests.
  raw_ptr<Profile> profile_;

  // The current gen204 id for logging, set by the overlay controller.
  uint64_t gen204_id_;

  // Each gen204 loader in this vector corresponds to an outstanding gen204
  // request. Storing them ensures they do not get deleted immediately
  // after being issued, which cancels the request.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> gen204_loaders_;

  base::WeakPtrFactory<LensOverlayGen204Controller> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_GEN204_CONTROLLER_H_
