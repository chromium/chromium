// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_frame_provider.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/cma/test/frame_generator_for_test.h"
#include "chromecast/media/cma/test/mock_frame_consumer.h"
#include "chromecast/media/cma/test/mock_frame_provider.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class BufferingFrameProviderTest : public testing::Test {
 public:
  BufferingFrameProviderTest();

  BufferingFrameProviderTest(const BufferingFrameProviderTest&) = delete;
  BufferingFrameProviderTest& operator=(const BufferingFrameProviderTest&) =
      delete;

  ~BufferingFrameProviderTest() override;

  // Setup the test.
  void Configure(
      size_t frame_count,
      const std::vector<bool>& provider_delayed_pattern,
      const std::vector<bool>& consumer_delayed_pattern);

  // Start the test.
  void Start();

  // Run the RunLoop and process messages
  void Run();

 protected:
  std::unique_ptr<BufferingFrameProvider> buffering_frame_provider_;
  std::unique_ptr<MockFrameConsumer> frame_consumer_;

 private:
  void OnTestTimeout();
  void OnTestCompleted();

  base::OnceClosure quit_closure_;
};

BufferingFrameProviderTest::BufferingFrameProviderTest() {
}

BufferingFrameProviderTest::~BufferingFrameProviderTest() {
}
void BufferingFrameProviderTest::Run() {
  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}
void BufferingFrameProviderTest::Configure(
    size_t frame_count,
    const std::vector<bool>& provider_delayed_pattern,
    const std::vector<bool>& consumer_delayed_pattern) {
  DCHECK_GE(frame_count, 1u);

  // Frame generation on the producer and consumer side.
  std::vector<FrameGeneratorForTest::FrameSpec> frame_specs(frame_count);
  for (size_t k = 0; k < frame_specs.size() - 1; k++) {
    frame_specs[k].has_config = (k == 0);
    frame_specs[k].timestamp = base::Milliseconds(40) * k;
    frame_specs[k].size = 512;
    frame_specs[k].has_decrypt_config = ((k % 3) == 0);
  }
  frame_specs.back().is_eos = true;

  std::unique_ptr<FrameGeneratorForTest> frame_generator_provider(
      new FrameGeneratorForTest(frame_specs));
  std::unique_ptr<FrameGeneratorForTest> frame_generator_consumer(
      new FrameGeneratorForTest(frame_specs));

  std::unique_ptr<MockFrameProvider> frame_provider(new MockFrameProvider());
  frame_provider->Configure(provider_delayed_pattern,
                            std::move(frame_generator_provider));

  size_t max_frame_size = 10 * 1024;
  size_t buffer_size = 10 * max_frame_size;
  buffering_frame_provider_.reset(new BufferingFrameProvider(
      std::unique_ptr<CodedFrameProvider>(frame_provider.release()),
      buffer_size, max_frame_size, BufferingFrameProvider::FrameBufferedCB()));

  frame_consumer_.reset(
      new MockFrameConsumer(buffering_frame_provider_.get()));
  frame_consumer_->Configure(consumer_delayed_pattern, false,
                             std::move(frame_generator_consumer));
}

void BufferingFrameProviderTest::Start() {
  frame_consumer_->Start(base::BindOnce(
      &BufferingFrameProviderTest::OnTestCompleted, base::Unretained(this)));
}

void BufferingFrameProviderTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  std::move(quit_closure_).Run();
}

void BufferingFrameProviderTest::OnTestCompleted() {
  std::move(quit_closure_).Run();
}

TEST_F(BufferingFrameProviderTest, FastProviderSlowConsumer) {
  bool provider_delayed_pattern[] = { false };
  bool consumer_delayed_pattern[] = { true };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + std::size(provider_delayed_pattern)),
            std::vector<bool>(consumer_delayed_pattern,
                              consumer_delayed_pattern +
                                  std::size(consumer_delayed_pattern)));

  base::test::SingleThreadTaskEnvironment task_environment;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BufferingFrameProviderTest::Start,
                                base::Unretained(this)));
  Run();
}

TEST_F(BufferingFrameProviderTest, SlowProviderFastConsumer) {
  bool provider_delayed_pattern[] = { true };
  bool consumer_delayed_pattern[] = { false };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + std::size(provider_delayed_pattern)),
            std::vector<bool>(consumer_delayed_pattern,
                              consumer_delayed_pattern +
                                  std::size(consumer_delayed_pattern)));

  base::test::SingleThreadTaskEnvironment task_environment;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BufferingFrameProviderTest::Start,
                                base::Unretained(this)));
  Run();
}

TEST_F(BufferingFrameProviderTest, SlowFastProducerConsumer) {
  // Lengths are prime between each other so we can test a lot of combinations.
  bool provider_delayed_pattern[] = {
    true, true, true, true, true,
    false, false, false, false
  };
  bool consumer_delayed_pattern[] = {
    true, true, true, true, true, true, true,
    false, false, false, false, false, false, false
  };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + std::size(provider_delayed_pattern)),
            std::vector<bool>(consumer_delayed_pattern,
                              consumer_delayed_pattern +
                                  std::size(consumer_delayed_pattern)));

  base::test::SingleThreadTaskEnvironment task_environment;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BufferingFrameProviderTest::Start,
                                base::Unretained(this)));
  Run();
}

}  // namespace media
}  // namespace chromecast
