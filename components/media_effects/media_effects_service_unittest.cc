// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_service.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom-forward.h"
#include "services/video_effects/test/fake_video_effects_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

namespace {

constexpr char kDeviceId[] = "test_device";

media::mojom::VideoEffectsConfigurationPtr GetConfigurationSync(
    mojo::Remote<media::mojom::VideoEffectsManager>& effects_manager) {
  base::test::TestFuture<media::mojom::VideoEffectsConfigurationPtr>
      output_configuration;
  effects_manager->GetConfiguration(output_configuration.GetCallback());
  return output_configuration.Take();
}

void SetFramingSync(
    mojo::Remote<media::mojom::VideoEffectsManager>& effects_manager,
    float framing_padding_ratio) {
  base::test::TestFuture<media::mojom::SetConfigurationResult> result_future;
  effects_manager->SetConfiguration(
      media::mojom::VideoEffectsConfiguration::New(
          nullptr, nullptr,
          media::mojom::Framing::New(gfx::InsetsF{framing_padding_ratio})),
      result_future.GetCallback());
  EXPECT_EQ(media::mojom::SetConfigurationResult::kOk, result_future.Get());
}
}  // namespace

class MediaEffectsServiceTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  user_prefs::TestBrowserContextWithPrefs browser_context_;
  MediaEffectsService service_{browser_context_.prefs()};
};

TEST_F(MediaEffectsServiceTest, BindVideoEffectsManager) {
  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(GetConfigurationSync(effects_manager)->framing.is_null());

  const float kFramingPaddingRatio = 0.2;
  SetFramingSync(effects_manager, kFramingPaddingRatio);

  auto configuration = GetConfigurationSync(effects_manager);
  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            configuration->framing->padding_ratios);
}

TEST_F(MediaEffectsServiceTest,
       BindVideoEffectsManager_TwoRegistrantsWithSameIdConnectToSameManager) {
  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager1;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager1.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;
  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager2;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager2.BindNewPipeAndPassReceiver());

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager2)->framing->padding_ratios);
}

TEST_F(
    MediaEffectsServiceTest,
    BindVideoEffectsManager_TwoRegistrantsWithDifferentIdConnectToDifferentManager) {
  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager1;
  service_.BindVideoEffectsManager(
      "test_device_1", effects_manager1.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;
  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager2;
  service_.BindVideoEffectsManager(
      "test_device_2", effects_manager2.BindNewPipeAndPassReceiver());

  // Expect `framing` to be unset because it is a separate instance of
  // `VideoEffectsManager`.
  auto framing = std::move(GetConfigurationSync(effects_manager2)->framing);
  EXPECT_TRUE(framing.is_null());
}

TEST_F(
    MediaEffectsServiceTest,
    OnLastReceiverDisconnected_ErasesTheManagerWhenAllReceiversAreDisconnected) {
  mojo::Remote<video_effects::mojom::VideoEffectsService> service;
  video_effects::FakeVideoEffectsService fake_effects_service(
      service.BindNewPipeAndPassReceiver());
  auto service_reset = SetVideoEffectsServiceRemoteForTesting(&service);

  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager1;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager1.BindNewPipeAndPassReceiver());
  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager2;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager2.BindNewPipeAndPassReceiver());

  auto effects_processor_future =
      fake_effects_service.GetEffectsProcessorCreationFuture();

  mojo::Remote<video_effects::mojom::VideoEffectsProcessor> effects_processor;
  service_.BindVideoEffectsProcessor(
      kDeviceId, effects_processor.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;

  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager2)->framing->padding_ratios);

  // Wait for the fake effects service to create the processor:
  EXPECT_TRUE(effects_processor_future->Wait());
  ASSERT_EQ(fake_effects_service.GetProcessors().size(), 1u);
  EXPECT_EQ(
      gfx::InsetsF{kFramingPaddingRatio},
      GetConfigurationSync(fake_effects_service.GetProcessors()[kDeviceId]
                               ->GetVideoEffectsManager())
          ->framing->padding_ratios);

  effects_manager1.reset();
  effects_manager2.reset();
  fake_effects_service.GetProcessors().erase(kDeviceId);
  // Wait for the reset to complete
  base::RunLoop().RunUntilIdle();

  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager3;
  service_.BindVideoEffectsManager(
      kDeviceId, effects_manager3.BindNewPipeAndPassReceiver());

  // Expect `framing` to be unset because it is a new instance of
  // `VideoEffectsManager`.
  EXPECT_TRUE(GetConfigurationSync(effects_manager3)->framing.is_null());
}

TEST_F(MediaEffectsServiceTest, BindVideoEffectsProcessor) {
  // Tests that `MediaEffectsService::BindVideoEffectsProcessor()` works, i.e.
  // causes the passed in remote to be connected.

  mojo::Remote<video_effects::mojom::VideoEffectsService> service;
  video_effects::FakeVideoEffectsService fake_effects_service(
      service.BindNewPipeAndPassReceiver());
  auto service_reset = SetVideoEffectsServiceRemoteForTesting(&service);

  auto effects_processor_future =
      fake_effects_service.GetEffectsProcessorCreationFuture();

  mojo::Remote<video_effects::mojom::VideoEffectsProcessor> effects_processor;
  service_.BindVideoEffectsProcessor(
      kDeviceId, effects_processor.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(effects_processor_future->Wait());
  EXPECT_TRUE(effects_processor.is_connected());
  EXPECT_EQ(fake_effects_service.GetProcessors().size(), 1u);
}

TEST_F(
    MediaEffectsServiceTest,
    BindVideoEffectsProcessor_TwoProcessorsWithDifferentIdConnectToDifferentManager) {
  // Tests that `MediaEffectsService::BindVideoEffectsProcessor()` connects to
  // a different manager if a different ID was used. This is validated by
  // checking that the managers return different configurations. We also set a
  // different config directly via effects manager interface (originating from
  // a call to `MediaEffectsService::BindVideoEffectsManager()`) so this test
  // also checks that a correct relationship is established between manager
  // and processor.

  mojo::Remote<video_effects::mojom::VideoEffectsService> service;
  video_effects::FakeVideoEffectsService fake_effects_service(
      service.BindNewPipeAndPassReceiver());
  auto service_reset = SetVideoEffectsServiceRemoteForTesting(&service);

  mojo::Remote<media::mojom::VideoEffectsManager> effects_manager;
  service_.BindVideoEffectsManager(
      "test_device_1", effects_manager.BindNewPipeAndPassReceiver());

  constexpr float kFramingPaddingRatio1 = 0.234;
  SetFramingSync(effects_manager, kFramingPaddingRatio1);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio1},
            GetConfigurationSync(effects_manager)->framing->padding_ratios);

  auto effects_processor_future1 =
      fake_effects_service.GetEffectsProcessorCreationFuture();

  mojo::Remote<video_effects::mojom::VideoEffectsProcessor> effects_processor1;
  service_.BindVideoEffectsProcessor(
      "test_device_2", effects_processor1.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(effects_processor_future1->Wait());
  ASSERT_EQ(fake_effects_service.GetProcessors().size(), 1u);

  constexpr float kFramingPaddingRatio2 = 0.345;
  SetFramingSync(fake_effects_service.GetProcessors()["test_device_2"]
                     ->GetVideoEffectsManager(),
                 kFramingPaddingRatio2);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio2},
            GetConfigurationSync(
                fake_effects_service.GetProcessors()["test_device_2"]
                    ->GetVideoEffectsManager())
                ->framing->padding_ratios);

  auto effects_processor_future2 =
      fake_effects_service.GetEffectsProcessorCreationFuture();

  mojo::Remote<video_effects::mojom::VideoEffectsProcessor> effects_processor2;
  service_.BindVideoEffectsProcessor(
      "test_device_3", effects_processor2.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(effects_processor_future2->Wait());
  ASSERT_EQ(fake_effects_service.GetProcessors().size(), 2u);

  constexpr float kFramingPaddingRatio3 = 0.456;
  SetFramingSync(fake_effects_service.GetProcessors()["test_device_3"]
                     ->GetVideoEffectsManager(),
                 kFramingPaddingRatio3);

  auto padding2 =
      std::move(GetConfigurationSync(
                    fake_effects_service.GetProcessors()["test_device_2"]
                        ->GetVideoEffectsManager())
                    ->framing->padding_ratios);
  auto padding3 =
      std::move(GetConfigurationSync(
                    fake_effects_service.GetProcessors()["test_device_3"]
                        ->GetVideoEffectsManager())
                    ->framing->padding_ratios);

  EXPECT_NE(padding2, padding3);
  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio2}, padding2);
  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio3}, padding3);
}
