// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/content/mojom/subresource_filter.mojom.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "url/gurl.h"

namespace blink {
class WebDocumentSubresourceFilter;
}  // namespace blink

namespace subresource_filter {

class UnverifiedRulesetDealer;
class WebDocumentSubresourceFilterImpl;

// The renderer-side agent of ContentSubresourceFilterThrottleManager. There is
// one instance per RenderFrame, responsible for setting up the subresource
// filter for the ongoing provisional document load in the frame when instructed
// to do so by the manager.
class SubresourceFilterAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<SubresourceFilterAgent>,
      public mojom::SubresourceFilterAgent {
 public:
  // The |ruleset_dealer| must not be null and must outlive this instance. The
  // |render_frame| may be null in unittests.
  explicit SubresourceFilterAgent(content::RenderFrame* render_frame,
                                  UnverifiedRulesetDealer* ruleset_dealer);

  SubresourceFilterAgent(const SubresourceFilterAgent&) = delete;
  SubresourceFilterAgent& operator=(const SubresourceFilterAgent&) = delete;

  ~SubresourceFilterAgent() override;

  // Unit tests don't have a RenderFrame so the construction relies on virtual
  // methods on this class instead to inject test behaviour. That can't happen
  // in the constructor, so we need an Initialize() method.
  void Initialize();

 protected:
  // Below methods are protected virtual so they can be mocked out in tests.

  // Returns the URL of the currently committed document.
  virtual GURL GetDocumentURL();

  // Returns true if this agent is attached to a frame that is a subresource
  // filter child. See content_subresource_filter_throttle_manager.h for
  // details.
  virtual bool IsSubresourceFilterChild();
  virtual bool IsParentAdFrame();
  virtual bool IsProvisional();
  // This is only called by non-main frames since a child main frames (e.g.
  // Fenced Frame) will not be initialized on the same stack as the script that
  // created it.
  virtual bool IsFrameCreatedByAdScript();

  // Injects the provided subresource |filter| into the DocumentLoader
  // orchestrating the most recently created document.
  virtual void SetSubresourceFilterForCurrentDocument(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter);

  // Informs the browser that the first subresource load has been disallowed for
  // the most recently created document. Not called if all resources are
  // allowed.
  virtual void SignalFirstSubresourceDisallowedForCurrentDocument();

  // Sends statistics about the DocumentSubresourceFilter's work to the browser.
  virtual void SendDocumentLoadStatistics(
      const mojom::DocumentLoadStatistics& statistics);

  // Tells the browser that the renderer tagged the frame as an ad frame.  This
  // is not sent for frames tagged by the browser.
  virtual void SendFrameIsAd();

  // Tells the browser that the frame is a frame that was created by ad script.
  // Fenced frame roots do not call this as they're tagged via
  // DidCreateFencedFrame.
  virtual void SendFrameWasCreatedByAdScript();

  // True if the frame has been heuristically determined to be an ad frame.
  virtual bool IsAdFrame();

  virtual const std::optional<blink::FrameAdEvidence>& AdEvidence();
  virtual void SetAdEvidence(const blink::FrameAdEvidence& ad_evidence);

  // The browser will not inform the renderer of the (sub)frame's ad status and
  // evidence in the case of an initial synchronous commit to about:blank. We
  // thus fill in the frame's ad evidence and, if necessary, tag it as an ad.
  // Fenced frame roots do not call this as their ad evidence is initialized
  // via DidCreateFencedFrame.
  void SetAdEvidenceForInitialEmptySubframe();

  // mojom::SubresourceFilterAgent:
  void ActivateForNextCommittedLoad(
      mojom::ActivationStatePtr activation_state,
      const std::optional<blink::FrameAdEvidence>& ad_evidence) override;

 private:
  // Returns the activation state for the `render_frame` to inherit. Root frames
  // inherit from their opener frames, and child frames inherit from their
  // parent frames. Assumes that the parent/opener is in a local frame relative
  // to this one, upon construction.
  static mojom::ActivationState GetInheritedActivationState(
      content::RenderFrame* render_frame);

  void RecordHistogramsOnFilterCreation(
      const mojom::ActivationState& activation_state);
  void ResetInfoForNextDocument();

  virtual const mojom::ActivationState
  GetInheritedActivationStateForNewDocument();

  void ConstructFilter(const mojom::ActivationState activation_state,
                       const GURL& url);

  mojom::SubresourceFilterHost* GetSubresourceFilterHost();

  void OnSubresourceFilterAgentRequest(
      mojo::PendingAssociatedReceiver<mojom::SubresourceFilterAgent> receiver);

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateNewDocument() override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;
  void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) override;
  void OnOverlayPopupAdDetected() override;
  void OnLargeStickyAdDetected() override;
  void DidCreateFencedFrame(
      const blink::RemoteFrameToken& placeholder_token) override;

  // Owned by the ChromeContentRendererClient and outlives us.
  raw_ptr<UnverifiedRulesetDealer> ruleset_dealer_;

  mojom::ActivationState activation_state_for_next_document_;

  // Use associated interface to make sure mojo messages are ordered with regard
  // to legacy IPC messages.
  mojo::AssociatedRemote<mojom::SubresourceFilterHost> subresource_filter_host_;

  mojo::AssociatedReceiver<mojom::SubresourceFilterAgent> receiver_{this};

  base::WeakPtr<WebDocumentSubresourceFilterImpl>
      filter_for_last_created_document_;
  base::WeakPtrFactory<SubresourceFilterAgent> weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
