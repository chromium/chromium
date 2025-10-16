// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_service.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/cpp/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
#include "components/media_effects/media_effects_model_provider.h"
#include "services/video_effects/public/cpp/video_effects_service_host.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom-forward.h"
#include "services/video_effects/test/fake_video_effects_service.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
class FakeModelProvider : public MediaEffectsModelProvider {
 public:
  ~FakeModelProvider() override = default;

  // MediaEffectsModelProvider:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
    if (model_path_) {
      observer->OnBackgroundSegmentationModelUpdated(model_path_);
    }
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  // Sets the model path and notifies observers about it:
  void SetModelPath(std::optional<base::FilePath> model_path) {
    model_path_ = std::move(model_path);
    for (auto& observer : observers_) {
      observer.OnBackgroundSegmentationModelUpdated(model_path_);
    }
  }

  base::WeakPtr<FakeModelProvider> weak_ptr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::ObserverList<Observer> observers_;
  std::optional<base::FilePath> model_path_;

  // Must be last:
  base::WeakPtrFactory<FakeModelProvider> weak_ptr_factory_{this};
};
#endif

}  // namespace

class MediaEffectsServiceTest : public testing::Test {
 public:
  MediaEffectsServiceTest() {
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
    auto model_provider = std::make_unique<FakeModelProvider>();
    model_provider_ = model_provider->weak_ptr();
    service_.emplace(std::move(model_provider));
#else
    service_.emplace();
#endif
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::optional<MediaEffectsService> service_;
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  base::WeakPtr<FakeModelProvider> model_provider_;
#endif
};

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)

TEST_F(MediaEffectsServiceTest, ModelFileIsOpenedAndSentToVideoEffects) {
  constexpr char kFirstModelBytes[] = "abcdefgh";
  constexpr char kSecondModelBytes[] = "ijklmnop";

  mojo::Remote<video_effects::mojom::VideoEffectsService> service;
  video_effects::FakeVideoEffectsService fake_effects_service(
      service.BindNewPipeAndPassReceiver());
  auto service_reset =
      video_effects::SetVideoEffectsServiceRemoteForTesting(&service);

  // Setting the model file path for the first time propagates the model file to
  // Video Effects Service: Prepare model file:
  base::ScopedTempFile temporary_model_file_path1;
  ASSERT_TRUE(temporary_model_file_path1.Create());
  ASSERT_TRUE(
      base::WriteFile(temporary_model_file_path1.path(), kFirstModelBytes));

  // Set it on Media Effects Service:
  auto model_opened_future =
      fake_effects_service.GetBackgroundSegmentationModelFuture();
  model_provider_->SetModelPath(temporary_model_file_path1.path());

  auto model_file = model_opened_future.Take();
  EXPECT_TRUE(model_file.IsValid());

  // Validate that the contents match the contents of the model file:
  std::string contents(sizeof(kFirstModelBytes), '\0');
  ASSERT_TRUE(model_file.Read(0, base::as_writable_byte_span(contents)));
  EXPECT_STREQ(contents.data(), kFirstModelBytes);

  // Setting the model file path for the second time propagates the model file
  // to Video Effects Service: Prepare model file:
  base::ScopedTempFile temporary_model_file_path2;
  ASSERT_TRUE(temporary_model_file_path2.Create());
  ASSERT_TRUE(
      base::WriteFile(temporary_model_file_path2.path(), kSecondModelBytes));

  // Set it on Media Effects Service:
  model_opened_future =
      fake_effects_service.GetBackgroundSegmentationModelFuture();
  model_provider_->SetModelPath(temporary_model_file_path2.path());

  model_file = model_opened_future.Take();
  EXPECT_TRUE(model_file.IsValid());

  // Validate that the contents match the contents of the model file:
  contents.resize(sizeof(kSecondModelBytes));
  ASSERT_TRUE(model_file.Read(0, base::as_writable_byte_span(contents)));
  EXPECT_STREQ(contents.data(), kSecondModelBytes);

  // Setting the model file to a path that doesn't exist does not propagate the
  // model to Video Effects Service: Set invalid path to the model:
  base::ScopedTempDir temporary_directory;
  ASSERT_TRUE(temporary_directory.CreateUniqueTempDir());

  model_opened_future =
      fake_effects_service.GetBackgroundSegmentationModelFuture();
  model_provider_->SetModelPath(
      temporary_directory.GetPath().AppendASCII("should_not_exist.tmp"));

  model_file = model_opened_future.Take();
  EXPECT_FALSE(model_file.IsValid());

  model_opened_future =
      fake_effects_service.GetBackgroundSegmentationModelFuture();
  model_provider_->SetModelPath(std::nullopt);

  model_file = model_opened_future.Take();
  EXPECT_FALSE(model_file.IsValid());
}
#endif
