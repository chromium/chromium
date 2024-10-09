// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace fingerprinting_protection_filter {

class UnverifiedRulesetDealer;

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
  using ActivationCallback = base::OnceCallback<void(
      const subresource_filter::mojom::ActivationState&)>;

  using FilterCallback =
      base::OnceCallback<void(subresource_filter::LoadPolicy)>;

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

  void DidFinishLoad() override;

  // Used to delete `this` to avoid memory leaks and ensure `render_frame()` is
  // always valid.
  void OnDestruct() override;

  base::WeakPtr<RendererAgent> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used to signal to the remote host that a subresource load has been
  // disallowed; must be run on the main thread. Virtual to allow mocking in
  // tests.
  virtual void OnSubresourceDisallowed();

  // Callback for when activation returns from the browser after calling
  // `CheckActivation()`;
  void OnActivationComputed(
      subresource_filter::mojom::ActivationStatePtr activation_state);

  // Called by `RendererURLLoaderThrottles` to get activation state, with a
  // callback bound to the throttle's task runner to provide the result. Must be
  // run on the main thread.
  void GetActivationState(ActivationCallback callback);

  // Called by `RendererURLLoaderThrottles` to check a URL against the filter,
  // with a callback bound to the throttle's task runner to provide the result.
  // Must be run on the main thread.
  void CheckURL(const GURL& url,
                url_pattern_index::proto::ElementType element_type,
                FilterCallback callback);

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

  // Returns the activation state for the `render_frame` to inherit, or nullopt
  // if there is none. Root frames inherit from their opener frames, and child
  // frames inherit from their parent frames. Assumes that the parent/opener is
  // in a local frame relative to this one, upon construction.
  virtual std::optional<subresource_filter::mojom::ActivationState>
  GetInheritedActivationState();

  // Initiates the process of getting the activation state for the current
  // state. In prod, this involves communicating with the browser process.
  virtual void RequestActivationState();

  // Stores a `DocumentSubresourceFilter` to be used for filtering future
  // resource loads.
  virtual void SetFilter(
      std::unique_ptr<subresource_filter::DocumentSubresourceFilter> filter);

  // Sends statistics about the `DocumentSubresourceFilter`s work to the
  // browser.
  virtual void SendDocumentLoadStatistics(
      const subresource_filter::mojom::DocumentLoadStatistics& statistics);

 private:
  // Initializes `filter_`. Assumes that activation has been computed.
  void MaybeCreateNewFilter();

  // Remote used to pass messages to the browser-side `ThrottleManager`.
  mojo::AssociatedRemote<mojom::FingerprintingProtectionHost>
      fingerprinting_protection_host_;

  subresource_filter::mojom::ActivationState activation_state_;

  bool pending_activation_ = true;

  // Whether the browser has already been notified that a resource was
  // disallowed for the current `RenderFrame`. Needed on the browser for metrics
  // collection.
  bool notified_disallow_ = false;

  GURL current_document_url_;

  raw_ptr<UnverifiedRulesetDealer> ruleset_dealer_;

  // Will be conditionally initialized once the activation state is retrieved
  // from the browser.
  std::unique_ptr<subresource_filter::DocumentSubresourceFilter> filter_;

  // The set of callbacks that the agent has received from
  // `RendererURLLoaderThrottles` before activation was received from the
  // browser. All callbacks will be run once activation is computed, at which
  // point they will no longer accumulate.
  std::vector<ActivationCallback> pending_activation_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RendererAgent> weak_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_AGENT_H_
