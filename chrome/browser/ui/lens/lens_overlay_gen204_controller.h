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

  virtual ~LensOverlayGen204Controller();

  // The different latency types that can be logged.
  enum class LatencyType {
    // From when the overlay was invoked to when the initial cluster info
    // request was sent.
    kInvocationToInitialClusterInfoRequestSent = 0,

    // From when the overlay was invoked to when the initial full page
    // objects request was sent.
    kInvocationToInitialFullPageObjectsRequestSent = 1,

    // From when the overlay was invoked to when the initial interaction
    // request was sent.
    kInvocationToInitialInteractionRequestSent = 2,

    // From when the overlay was invoked to when the initial page content
    // request was sent.
    kInvocationToInitialPageContentRequestSent = 3,

    // From when the overlay was invoked to when the initial partial page
    // content request was sent.
    kInvocationToInitialPartialPageContentRequestSent = 9,

    // From when the full image request began processing to when the response
    // was received.
    kFullPageObjectsRequestFetchLatency = 4,

    // From when the full image translate request began processing to when the
    // response was received.
    kFullPageTranslateRequestFetchLatency = 5,

    // From when the interaction request began processing to when the response
    // was received.
    kInteractionRequestFetchLatency = 6,

    // From when the page content upload request began processing to when the
    // response was received.
    kPageContentUploadLatency = 7,

    // From the partial page content upload request began processing to when the
    // response was received.
    kPartialPageContentUploadLatency = 8,
  };

  // Sets the state of the controller. Should be called once
  // per query flow, at the start, when the Lens Overlay opens.
  void OnQueryFlowStart(lens::LensOverlayInvocationSource invocation_source,
                        Profile* profile,
                        uint64_t gen204_id);

  // Sends a Lens objects request latency gen204 request.
  void SendLatencyGen204IfEnabled(
      LatencyType latency_type,
      base::TimeDelta latency_duration,
      std::string vit_query_param_value,
      std::optional<base::TimeDelta> cluster_info_latency,
      std::optional<std::string> encoded_analytics_id);

  // Sends a task completion gen204 request. The analytics id is the
  // latest Lens request analytics id from the query controller.
  // The user action is the action that triggered the task completion
  // event.
  void SendTaskCompletionGen204IfEnabled(std::string encoded_analytics_id,
                                         lens::mojom::UserAction user_action);

  // Sends a semantic event gen204 request.
  void SendSemanticEventGen204IfEnabled(lens::mojom::SemanticEvent event);

  // Sends any final gen204 requests and marks the end of the query flow.
  // Called when the Lens Overlay is closed.
  // The analytics id is the latest Lens request analytics id from the
  // query controller.
  void OnQueryFlowEnd(std::string encoded_analytics_id);

  // Issues the gen204 network request and adds a loader to gen204_loaders_.
  // Checks that the user is opted into metrics logging.
  // Can be overridden for testing.
  virtual void CheckMetricsConsentAndIssueGen204NetworkRequest(GURL url);

 private:
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
