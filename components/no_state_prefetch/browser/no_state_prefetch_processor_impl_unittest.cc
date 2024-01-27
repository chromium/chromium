// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/system/functions.h"
#include "third_party/blink/public/common/features.h"

namespace prerender {

class MockNoStatePrefetchLinkManager final : public NoStatePrefetchLinkManager {
 public:
  MockNoStatePrefetchLinkManager()
      : NoStatePrefetchLinkManager(/*manager=*/nullptr) {}

  std::optional<int> OnStartLinkTrigger(
      int launcher_render_process_id,
      int launcher_render_view_id,
      int launcher_render_frame_id,
      blink::mojom::PrerenderAttributesPtr attributes,
      const url::Origin& initiator_origin) override {
    DCHECK(!is_start_called_);
    is_start_called_ = true;
    return link_trigger_id_;
  }

  void OnCancelLinkTrigger(int link_trigger_id) override {
    DCHECK_EQ(link_trigger_id_, link_trigger_id);
    DCHECK(!is_cancel_called_);
    is_cancel_called_ = true;
  }

  void OnAbandonLinkTrigger(int link_trigger_id) override {
    DCHECK_EQ(link_trigger_id_, link_trigger_id);
    DCHECK(!is_abandon_called_);
    is_abandon_called_ = true;
  }

  bool is_start_called() const { return is_start_called_; }
  bool is_cancel_called() const { return is_cancel_called_; }
  bool is_abandon_called() const { return is_abandon_called_; }

 private:
  const int link_trigger_id_ = 100;
  bool is_start_called_ = false;
  bool is_cancel_called_ = false;
  bool is_abandon_called_ = false;
};

class MockNoStatePrefetchProcessorImplDelegate final
    : public NoStatePrefetchProcessorImplDelegate {
 public:
  explicit MockNoStatePrefetchProcessorImplDelegate(
      MockNoStatePrefetchLinkManager* link_manager)
      : link_manager_(link_manager) {}

  NoStatePrefetchLinkManager* GetNoStatePrefetchLinkManager(
      content::BrowserContext* browser_context) override {
    return link_manager_;
  }

 private:
  raw_ptr<MockNoStatePrefetchLinkManager, AcrossTasksDanglingUntriaged>
      link_manager_;
};

class NoStatePrefetchProcessorImplTest
    : public content::RenderViewHostTestHarness {};

TEST_F(NoStatePrefetchProcessorImplTest, StartCancelAbandon) {
  auto link_manager = std::make_unique<MockNoStatePrefetchLinkManager>();

  mojo::Remote<blink::mojom::NoStatePrefetchProcessor> remote;
  NoStatePrefetchProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockNoStatePrefetchProcessorImplDelegate>(
          link_manager.get()));

  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = GURL("https://example.com/prefetch");
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_start_called());
  remote->Start(std::move(attributes));
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

TEST_F(NoStatePrefetchProcessorImplTest, StartAbandon) {
  auto link_manager = std::make_unique<MockNoStatePrefetchLinkManager>();

  mojo::Remote<blink::mojom::NoStatePrefetchProcessor> remote;
  NoStatePrefetchProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockNoStatePrefetchProcessorImplDelegate>(
          link_manager.get()));

  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = GURL("https://example.com/prefetch");
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_start_called());
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_TRUE(link_manager->is_start_called());

  // Connection lost should abandon the link manager.
  EXPECT_FALSE(link_manager->is_abandon_called());
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(link_manager->is_abandon_called());
}

TEST_F(NoStatePrefetchProcessorImplTest, StartTwice) {
  auto link_manager = std::make_unique<MockNoStatePrefetchLinkManager>();

  mojo::Remote<blink::mojom::NoStatePrefetchProcessor> remote;
  NoStatePrefetchProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockNoStatePrefetchProcessorImplDelegate>(
          link_manager.get()));

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = GURL("https://example.com/prefetch");
  attributes1->referrer = blink::mojom::Referrer::New();

  // Start() call should be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_start_called());
  remote->Start(std::move(attributes1));
  remote.FlushForTesting();
  EXPECT_TRUE(link_manager->is_start_called());

  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = GURL("https://example.com/prefetch");
  attributes2->referrer = blink::mojom::Referrer::New();

  // Call Start() again. This should be reported as a bad mojo message.
  ASSERT_TRUE(bad_message_error.empty());
  remote->Start(std::move(attributes2));
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "NSPPI_START_TWICE");
  // Clean up error handler, to avoid causing other tests run in the same
  // process from crashing.
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_F(NoStatePrefetchProcessorImplTest, Cancel) {
  auto link_manager = std::make_unique<MockNoStatePrefetchLinkManager>();

  mojo::Remote<blink::mojom::NoStatePrefetchProcessor> remote;
  NoStatePrefetchProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockNoStatePrefetchProcessorImplDelegate>(
          link_manager.get()));

  // Call Cancel() before Start().
  EXPECT_FALSE(link_manager->is_cancel_called());
  remote->Cancel();
  remote.FlushForTesting();
  // The cancellation should not be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_cancel_called());
}

TEST_F(NoStatePrefetchProcessorImplTest, Abandon) {
  auto link_manager = std::make_unique<MockNoStatePrefetchLinkManager>();

  mojo::Remote<blink::mojom::NoStatePrefetchProcessor> remote;
  NoStatePrefetchProcessorImpl::Create(
      main_rfh(), remote.BindNewPipeAndPassReceiver(),
      std::make_unique<MockNoStatePrefetchProcessorImplDelegate>(
          link_manager.get()));

  // Disconnect before Start().
  EXPECT_FALSE(link_manager->is_abandon_called());
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  // The disconnection should not be propagated to the link manager.
  EXPECT_FALSE(link_manager->is_abandon_called());
}

}  // namespace prerender
