// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/video_effects_manager_impl.h"

#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

namespace {
media::mojom::VideoEffectsConfigurationPtr GetConfigurationSync(
    mojo::Remote<media::mojom::ReadonlyVideoEffectsManager>& effects_manager) {
  base::test::TestFuture<media::mojom::VideoEffectsConfigurationPtr>
      output_configuration;
  effects_manager->GetConfiguration(output_configuration.GetCallback());
  return output_configuration.Take();
}

class ConfigurationObserverImpl
    : public media::mojom::VideoEffectsConfigurationObserver {
  using GetConfigChangesCallback =
      base::RepeatingCallback<void(media::mojom::VideoEffectsConfigurationPtr)>;

 public:
  ConfigurationObserverImpl() : receiver_(this) {}

  void OnConfigurationChanged(
      media::mojom::VideoEffectsConfigurationPtr configuration) override {
    on_get_config_changes_callback_.Run(configuration.Clone());
  }

  void SetOnGetConfigChangesCallback(GetConfigChangesCallback callback) {
    on_get_config_changes_callback_ = std::move(callback);
  }

  mojo::PendingRemote<media::mojom::VideoEffectsConfigurationObserver>
  GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  GetConfigChangesCallback on_get_config_changes_callback_;

  mojo::Receiver<media::mojom::VideoEffectsConfigurationObserver> receiver_;
};
}  // namespace

class VideoEffectsManagerImplTest : public testing::Test {
 public:
  VideoEffectsManagerImplTest()
      : effects_manager_(
            nullptr,
            base::BindOnce(
                &VideoEffectsManagerImplTest::OnLastReceiverDisconnected,
                base::Unretained(this))) {}

  mojo::Remote<media::mojom::ReadonlyVideoEffectsManager>
  GetEffectManagerRemote() {
    mojo::Remote<media::mojom::ReadonlyVideoEffectsManager> remote;
    effects_manager_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void SetConfiguration(
      media::mojom::VideoEffectsConfigurationPtr configuration) {
    effects_manager_.SetConfiguration(std::move(configuration));
  }

  void OnLastReceiverDisconnected() { ++last_receiver_disconnect_count_; }

 protected:
  size_t last_receiver_disconnect_count_ = 0;

 private:
  VideoEffectsManagerImpl effects_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(VideoEffectsManagerImplTest, SetGetConfiguration) {
  auto remote = GetEffectManagerRemote();

  const float kPaddingRatio = 0.342;
  SetConfiguration(media::mojom::VideoEffectsConfiguration::New(
      nullptr, nullptr,
      media::mojom::Framing::New(gfx::InsetsF{kPaddingRatio})));

  EXPECT_EQ(kPaddingRatio,
            GetConfigurationSync(remote)->framing->padding_ratios.top());
}

TEST_F(VideoEffectsManagerImplTest, AddObserver) {
  auto remote = GetEffectManagerRemote();

  ConfigurationObserverImpl configuration_observer;

  base::test::TestFuture<media::mojom::VideoEffectsConfigurationPtr>
      result_future;
  configuration_observer.SetOnGetConfigChangesCallback(
      result_future.GetRepeatingCallback());

  remote->AddObserver(configuration_observer.GetRemote());

  for (const auto& padding_ratio :
       std::vector<float>{0.1, 0.2343, 0.3435, 0.38500}) {
    SetConfiguration(media::mojom::VideoEffectsConfiguration::New(
        nullptr, nullptr,
        media::mojom::Framing::New(gfx::InsetsF{padding_ratio})));

    EXPECT_EQ(padding_ratio,
              result_future.Take()->framing->padding_ratios.top());
  }
}
