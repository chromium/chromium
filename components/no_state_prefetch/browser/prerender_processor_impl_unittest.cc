// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/prerender_processor_impl.h"

#include "base/run_loop.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"
#include "components/no_state_prefetch/browser/prerender_processor_impl_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/features.h"

namespace prerender {

class MockPrerenderLinkManager final : public PrerenderLinkManager {
 public:
  MockPrerenderLinkManager() : PrerenderLinkManager(/*manager=*/nullptr) {}

  base::Optional<int> OnStartPrerender(
      int launcher_render_process_id,
      int launcher_render_view_id,
      blink::mojom::PrerenderAttributesPtr attributes,
      const url::Origin& initiator_origin,
      mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
          processor_client) override {
    DCHECK(!is_start_called_);
    is_start_called_ = true;
    return prerender_id_;
  }

  void OnCancelPrerender(int prerender_id) override {
    DCHECK_EQ(prerender_id_, prerender_id);
    DCHECK(!is_cancel_called_);
    is_cancel_called_ = true;
  }

  void OnAbandonPrerender(int prerender_id) override {
    DCHECK_EQ(prerender_id_, prerender_id);
    DCHECK(!is_abandon_called_);
    is_abandon_called_ = true;
  }

  bool is_start_called() const { return is_start_called_; }
  bool is_cancel_called() const { return is_cancel_called_; }
  bool is_abandon_called() const { return is_abandon_called_; }

 private:
  const int prerender_id_ = 100;
  bool is_start_called_ = false;
  bool is_cancel_called_ = false;
  bool is_abandon_called_ = false;
};

class MockPrerenderProcessorImplDelegate final
    : public PrerenderProcessorImplDelegate {
 public:
  explicit MockPrerenderProcessorImplDelegate(
      MockPrerenderLinkManager* link_manager)
      : link_manager_(link_manager) {}

  PrerenderLinkManager* GetPrerenderLinkManager(
      content::BrowserContext* browser_context) override {
    return link_manager_;
  }

 private:
  MockPrerenderLinkManager* link_manager_;
};

class PrerenderProcessorImplTest
    : public content::RenderViewHostTestHarness,
      public blink::mojom::PrerenderProcessorClient {
 public:
  // blink::mojom::PrerenderProcessorClient:
  void OnPrerenderStart() override {}
  void OnPrerenderStopLoading() override {}
  void OnPrerenderDomContentLoaded() override {}
  void OnPrerenderStop() override {}

 protected:
  mojo::Receiver<blink::mojom::PrerenderProcessorClient> receiver_{this};
};

TEST_F(PrerenderProcessorImplTest, StartCancelAbandon) {
  auto link_manager = std::make_unique<MockPrerenderLinkManager>();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  PrerenderProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockPrerenderProcessorImplDelegate>(link_manager.get()));

  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = GURL("https://example.com/prefetch");
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_start_called());
  remote->Start(std::move(attributes), receiver_.BindNewPipeAndPassRemote());
  remote.FlushForTesting();
  EXPECT_TRUE(link_manager->is_start_called());

  // Cancel() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_cancel_called());
  remote->Cancel();
  remote.FlushForTesting();
  EXPECT_TRUE(link_manager->is_cancel_called());

  // Connection lost should abandon the link manager.
  EXPECT_FALSE(link_manager->is_abandon_called());
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(link_manager->is_abandon_called());
}

TEST_F(PrerenderProcessorImplTest, StartAbandon) {
  auto link_manager = std::make_unique<MockPrerenderLinkManager>();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  PrerenderProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockPrerenderProcessorImplDelegate>(link_manager.get()));

  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = GURL("https://example.com/prefetch");
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_start_called());
  remote->Start(std::move(attributes), receiver_.BindNewPipeAndPassRemote());
  remote.FlushForTesting();
  EXPECT_TRUE(link_manager->is_start_called());

  // Connection lost should abandon the link manager.
  EXPECT_FALSE(link_manager->is_abandon_called());
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(link_manager->is_abandon_called());
}

TEST_F(PrerenderProcessorImplTest, Cancel) {
  auto link_manager = std::make_unique<MockPrerenderLinkManager>();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  PrerenderProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockPrerenderProcessorImplDelegate>(link_manager.get()));

  // Call Cancel() before Start().
  EXPECT_FALSE(link_manager->is_cancel_called());
  remote->Cancel();
  remote.FlushForTesting();
  // The cancellation should not be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_cancel_called());
}

TEST_F(PrerenderProcessorImplTest, Abandon) {
  auto link_manager = std::make_unique<MockPrerenderLinkManager>();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  PrerenderProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockPrerenderProcessorImplDelegate>(link_manager.get()));

  // Disconnect before Start().
  EXPECT_FALSE(link_manager->is_abandon_called());
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  // The disconnection should not be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_abandon_called());
}

}  // namespace prerender
