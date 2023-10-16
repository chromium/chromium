// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/common/prerender_url_loader_throttle.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {
namespace {

class PrerenderURLLoaderThrottleTest : public testing::Test {
 public:
  PrerenderURLLoaderThrottleTest() = default;
  ~PrerenderURLLoaderThrottleTest() override = default;

  PrerenderURLLoaderThrottleTest(const PrerenderURLLoaderThrottleTest&) =
      delete;
  PrerenderURLLoaderThrottleTest& operator=(
      const PrerenderURLLoaderThrottleTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

class FakeCanceler : public prerender::mojom::PrerenderCanceler {
 public:
  FakeCanceler() = default;
  ~FakeCanceler() override = default;
  void CancelPrerenderForUnsupportedScheme() override {}
  void CancelPrerenderForNoStatePrefetch() override {}
};

TEST_F(PrerenderURLLoaderThrottleTest, DestructionClosure) {
  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeCanceler>(),
                              pending_remote.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<PrerenderURLLoaderThrottle> prerender_throttle =
      std::make_unique<PrerenderURLLoaderThrottle>("PREFIX",
                                                   std::move(pending_remote));
  bool destruction_closure_called = false;
  prerender_throttle->set_destruction_closure(
      base::BindLambdaForTesting([&]() { destruction_closure_called = true; }));
  EXPECT_FALSE(destruction_closure_called);
  prerender_throttle.reset();
  EXPECT_TRUE(destruction_closure_called);
}

TEST_F(PrerenderURLLoaderThrottleTest,
       DestructionClosureAfterDetachFromCurrentSequence) {
  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeCanceler>(),
                              pending_remote.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<PrerenderURLLoaderThrottle> prerender_throttle =
      std::make_unique<PrerenderURLLoaderThrottle>("PREFIX",
                                                   std::move(pending_remote));
  base::RunLoop run_loop;
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  prerender_throttle->set_destruction_closure(base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(current_task_runner->RunsTasksInCurrentSequence());
    run_loop.Quit();
  }));

  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      std::move(prerender_throttle);
  throttle->DetachFromCurrentSequence();

  // Post a task so that the throttle is destroyed on a different sequence,
  // and then ensure the destruction closure is called on the current
  // sequence, not the different sequence.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<blink::URLLoaderThrottle> throttle) {
                       throttle.reset();
                     },
                     std::move(throttle)));
  run_loop.Run();
}

}  // namespace
}  // namespace prerender
