// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace fingerprinting_protection_filter {

// Orchestrates the interface between the browser-side
// Fingerprinting Protection Filter classes and a single `RenderFrame`. Deals
// with requesting the current activation state from the browser and keeping it
// up-to-date in the event of changes to the current page. Also notifies
// `RendererURLLoaderThrottles` of activation state and attaches a handle to a
// filter to the current `DocumentLoader` when activated.
class RendererAgent : public content::RenderFrameObserver,
                      public content::RenderFrameObserverTracker<RendererAgent>,
                      public mojom::FingerprintingProtectionAgent {
 public:
  explicit RendererAgent(content::RenderFrame* render_frame);

  RendererAgent(const RendererAgent&) = delete;
  RendererAgent& operator=(const RendererAgent&) = delete;

  ~RendererAgent() override;

  // Unit tests don't have a `RenderFrame` so the construction relies on virtual
  // methods on this class instead to inject test behaviour. That can't happen
  // in the constructor, so we need an `Initialize()` method.
  void Initialize();

  // content::RenderFrameObserver:
  void DidCreateNewDocument() override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;

  // Used to delete `this` to avoid memory leaks and ensure `render_frame()` is
  // always valid.
  void OnDestruct() override;

  base::WeakPtr<RendererAgent> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Returns a callback that will run on the main thread.
  using OnSubresourceEvaluatedCallback = base::RepeatingCallback<void(
      const GURL& url,
      const std::optional<std::string>& devtools_request_id,
      bool subresource_disallowed,
      const subresource_filter::mojom::DocumentLoadStatistics& statistics)>;
  OnSubresourceEvaluatedCallback GetOnSubresourceCallback();

  // Called by RendererUrlLoaderThrottle when a subresource is evaluated.
  void OnSubresourceEvaluated(
      const GURL& url,
      const std::optional<std::string>& devtools_request_id,
      bool subresource_disallowed,
      const subresource_filter::mojom::DocumentLoadStatistics& statistics);

  // Used to aggregate statistics for the current document load; must be run on
  // the main thread.
  void OnSubresourceEvaluatedImpl(
      const subresource_filter::mojom::DocumentLoadStatistics& statistics);

  // Used to signal to the remote host that a subresource load has been
  // disallowed; must be run on the main thread. Virtual to allow mocking in
  // tests.
  virtual void OnSubresourceDisallowed(
      const GURL& url,
      const std::optional<std::string>& devtools_request_id);

  // mojom::FingerprintingProtectionAgent:
  void ActivateForNextCommittedLoad(
      subresource_filter::mojom::ActivationStatePtr activation_state) override;

  // Called by `RendererURLLoaderThrottle`s to register themselves to receive
  // activation state from this `RendererAgent`. Must be run on the main thread.
  // Virtual for testing.
  using ActivationComputedCallback = base::OnceCallback<void(
      subresource_filter::mojom::ActivationState activation_state,
      OnSubresourceEvaluatedCallback on_subresource_evaluated_callback,
      const GURL& current_document_url)>;
  virtual void AddActivationComputedCallback(
      ActivationComputedCallback activation_computed_callback);

 protected:
  // The below methods are protected virtual so they can be mocked out in tests.

  // Returns the URL of the currently-committed main frame `Document`.
  virtual GURL GetMainDocumentUrl();

  // Returns whether the current frame is the outermost main frame of the
  // `Page`.
  virtual bool IsTopLevelMainFrame();

  // Returns whether the current frame has an opener in a process-local frame
  // that it can attempt to inherit activation from.
  virtual bool HasValidOpener();

  // Returns the current host mojo pipe endpoint or attempts to initialize it
  // from the `RenderFrame` if there is none.
  virtual mojom::FingerprintingProtectionHost*
  GetFingerprintingProtectionHost();

  void OnFingerprintingProtectionAgentRequest(
      mojo::PendingAssociatedReceiver<mojom::FingerprintingProtectionAgent>
          receiver);

  // Returns the activation state for the `render_frame` to inherit, or nullopt
  // if there is none. Root frames inherit from their opener frames, and child
  // frames inherit from their parent frames. Assumes that the parent/opener is
  // in a local frame relative to this one, upon construction.
  virtual std::optional<subresource_filter::mojom::ActivationState>
  GetInheritedActivationState();

  // Sends statistics about the `DocumentSubresourceFilter`s work to the
  // browser.
  virtual void SendDocumentLoadStatistics(
      const subresource_filter::mojom::DocumentLoadStatistics& statistics);

  // The activation state for the current page, received from the browser.
  // Note that the `RendererAgent` covers a single RenderFrame at a time, which
  // may be the main frame or a subframe within a larger page.
  subresource_filter::mojom::ActivationState
      activation_state_for_next_document_;

  // The most recent activation state that has been sent to
  // `RendererURLLoaderThrottle`s and should be used for filtering. Differs from
  // activation_state_for_next_document_ in that the presence of this state
  // indicates a document has been created within this agent's frame.
  std::optional<subresource_filter::mojom::ActivationState>
      activation_state_to_inherit_;

  // Aggregates statistics from all throttles before sending to the browser.
  subresource_filter::mojom::DocumentLoadStatistics
      aggregated_document_statistics_;

 private:
  // Sends activation to any pending throttles and saves the most recent
  // activation state from the browser to possibly be inherited by the next
  // document/frame.
  void MaybeSendActivationToThrottles();

  void SendActivationToAllPendingThrottles();

  // Remote used to pass messages to the browser-side `ThrottleManager`.
  mojo::AssociatedRemote<mojom::FingerprintingProtectionHost>
      fingerprinting_protection_host_;
  mojo::AssociatedReceiver<mojom::FingerprintingProtectionAgent> receiver_{
      this};

  // Whether activation state has been received from the browser or through
  // inheritance from an ancestor frame in the tree.
  bool pending_activation_ = true;

  // Whether the browser has already been notified that a resource was
  // disallowed for the current `RenderFrame`. Needed on the browser for metrics
  // collection.
  bool notified_disallow_ = false;

  GURL current_document_url_;

  // A list of `RendererURLLoaderThrottle`s callbacks whose throttle is active
  // on the current `RenderFrame` and that are waiting for activation decisions
  // from this `RendererAgent`.
  std::vector<ActivationComputedCallback> activation_computed_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RendererAgent> weak_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
