// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_manager_binder.h"
#include "base/test/test_future.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

namespace {
video_capture::mojom::VideoEffectsConfigurationPtr GetConfigurationSync(
    mojo::Remote<video_capture::mojom::VideoEffectsManager>& effect_manager) {
  base::test::TestFuture<video_capture::mojom::VideoEffectsConfigurationPtr>
      output_configuration;
  effect_manager->GetConfiguration(output_configuration.GetCallback());
  return output_configuration.Take();
}
}  // namespace

class MediaEffectsManagerBinderTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  user_prefs::TestBrowserContextWithPrefs browser_context_;
};

TEST_F(MediaEffectsManagerBinderTest, BindVideoEffectsManager) {
  const char* kDeviceId = "device_id";

  mojo::Remote<video_capture::mojom::VideoEffectsManager> video_effects_manager;
  media_effects::BindVideoEffectsManager(
      kDeviceId, &browser_context_,
      video_effects_manager.BindNewPipeAndPassReceiver());

  // Allow queued device registration to complete.
  base::RunLoop().RunUntilIdle();

  const float kPaddingRatio = 0.383;
  base::test::TestFuture<video_capture::mojom::SetConfigurationResult>
      result_future;
  video_effects_manager->SetConfiguration(
      video_capture::mojom::VideoEffectsConfiguration::New(
          nullptr, nullptr,
          video_capture::mojom::Framing::New(gfx::InsetsF{kPaddingRatio})),
      result_future.GetCallback());
  EXPECT_EQ(video_capture::mojom::SetConfigurationResult::kOk,
            result_future.Get());

  EXPECT_EQ(kPaddingRatio, GetConfigurationSync(video_effects_manager)
                               ->framing->padding_ratios.top());
}
