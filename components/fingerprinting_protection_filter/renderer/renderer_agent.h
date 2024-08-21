// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace subresource_filter {
class WebDocumentSubresourceFilterImpl;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

class UnverifiedRulesetDealer;
class RendererURLLoaderThrottle;

// Orchestrates the interface between the browser-side
// Fingerprinting Protection Filter classes and a single `RenderFrame`. Deals
// with requesting the current activation state from the browser and keeping it
// up-to-date in the event of changes to the current page. Also notifies
// `RendererURLLoaderThrottles` of activation state and attaches a handle to a
// filter to the current `DocumentLoader` when activated.
class RendererAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<RendererAgent> {
 public:
  // The `ruleset_dealer` must not be null and must outlive this instance.
  RendererAgent(content::RenderFrame* render_frame,
                UnverifiedRulesetDealer* ruleset_dealer);

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

  void WillDetach(blink::DetachReason detach_reason) override;

  void OnDestruct() override;

  // Functions to keep track of active throttles. `AddThrottle()` should be
  // called whenever a new `RendererURLLoaderThrottle` is created and
  // `DeleteThrottle()` should be called in its destructor.
  void AddThrottle(RendererURLLoaderThrottle* throttle);

  void DeleteThrottle(RendererURLLoaderThrottle* throttle);

  // Used to signal to the remote host that a subresource load has been
  // disallowed. Virtual to allow mocking in tests.
  virtual void OnSubresourceDisallowed();

  // Returns the current host mojo pipe endpoint or attempts to initialize it
  // from the `RenderFrame` if there is none.
  mojom::FingerprintingProtectionHost* GetFingerprintingProtectionHost();

  // Callback for when activation returns from the browser after calling
  // `CheckActivation()`;
  void OnActivationComputed(
      subresource_filter::mojom::ActivationStatePtr activation_state);

  bool IsPendingActivation() { return pending_activation_; }

  subresource_filter::mojom::ActivationState GetActivationState() {
    return activation_state_;
  }

 protected:
  // The below methods are protected virtual so they can be mocked out in tests.

  // Returns the URL of the currently-committed main frame `Document`.
  virtual GURL GetMainDocumentUrl();

  // Returns whether the current frame is the outermost main frame of the
  // `Page`.
  virtual bool IsTopLevelMainFrame();

  // Initiates the process of getting the activation state for the current
  // state. In prod, this involves communicating with the browser process.
  virtual void RequestActivationState();

  // Injects the provided filter into the current `DocumentLoader`.
  virtual void SetFilterForCurrentDocument(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter);

 private:
  // Returns the activation state for the `render_frame` to inherit. Root frames
  // inherit from their opener frames, and child frames inherit from their
  // parent frames. Assumes that the parent/opener is in a local frame relative
  // to this one, upon construction.
  static subresource_filter::mojom::ActivationState GetInheritedActivationState(
      content::RenderFrame* render_frame);

  // Initializes `filter_`. Assumes that activation has been computed.
  void MaybeCreateNewFilter();

  // Removes all throttle pointers from `throttles_`. Called in response to the
  // `RendererAgent` being destroyed or the frame being reset (i.e. when a new
  // document is created).
  void DeleteAllThrottles();

  // Remote used to pass messages to the browser-side `ThrottleManager`.
  mojo::AssociatedRemote<mojom::FingerprintingProtectionHost>
      fingerprinting_protection_host_;

  subresource_filter::mojom::ActivationState activation_state_;

  bool pending_activation_ = true;

  GURL current_document_url_;

  raw_ptr<UnverifiedRulesetDealer> ruleset_dealer_;

  base::WeakPtr<subresource_filter::WebDocumentSubresourceFilterImpl> filter_;

  // The set of all ongoing URLLoaderThrottles for filtering subresources on
  // the current renderer.
  base::flat_set<raw_ptr<RendererURLLoaderThrottle>> throttles_;

  base::WeakPtrFactory<RendererAgent> weak_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
