// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/volume_map.h"

#include <string>

#include "base/check.h"
#include "base/test/values_test_util.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

const float kEpsilon = 0.0001;
const char* kNewVolumeMap = R"json({"volume_map": [
    {"level":0.0, "db":-120.0},
    {"level":0.5, "db":-10.0},
    {"level":1.0, "db":0.0}
  ]}
)json";

class TestFileProvider : public CastAudioJsonProvider {
 public:
  TestFileProvider(const std::string& file_contents)
      : file_contents_(file_contents) {}

  TestFileProvider(const TestFileProvider&) = delete;
  TestFileProvider& operator=(const TestFileProvider&) = delete;

  ~TestFileProvider() override = default;

  void CallTuningChangedCallback(const std::string& new_config) {
    DCHECK(callback_);
    callback_.Run(base::test::ParseJsonDict(new_config));
  }

 private:
  std::optional<base::Value::Dict> GetCastAudioConfig() override {
    return base::test::ParseJsonDict(file_contents_);
  }

  void SetTuningChangedCallback(TuningChangedCallback callback) override {
    callback_ = std::move(callback);
  }

  const std::string file_contents_;
  TuningChangedCallback callback_;
};

TEST(VolumeMapTest, UsesDefaultMapIfConfigEmpty) {
  VolumeMap volume_map(std::make_unique<TestFileProvider>("{}"));
  EXPECT_NEAR(-58.0f, volume_map.VolumeToDbFS(0.01f), kEpsilon);
  EXPECT_NEAR(-48.0f, volume_map.VolumeToDbFS(1.0 / 11.0), kEpsilon);
  EXPECT_NEAR(-8.0f, volume_map.VolumeToDbFS(9.0 / 11.0), kEpsilon);
  EXPECT_NEAR(-0.0f, volume_map.VolumeToDbFS(1.0f), kEpsilon);

  EXPECT_NEAR(0.01, volume_map.DbFSToVolume(-58.0), kEpsilon);
  EXPECT_NEAR(1.0 / 11.0, volume_map.DbFSToVolume(-48.0), kEpsilon);
  EXPECT_NEAR(9.0 / 11.0, volume_map.DbFSToVolume(-8.0), kEpsilon);
  EXPECT_NEAR(1.0, volume_map.DbFSToVolume(0.0), kEpsilon);
}

TEST(VolumeMapTest, LoadsInitialConfig) {
  VolumeMap volume_map(std::make_unique<TestFileProvider>(kNewVolumeMap));
  EXPECT_NEAR(-10.0f, volume_map.VolumeToDbFS(0.5), kEpsilon);
}

TEST(VolumeMapTest, VolumeToDbFSInterpolates) {
  VolumeMap volume_map(std::make_unique<TestFileProvider>(kNewVolumeMap));
  EXPECT_NEAR((-120.0 - 10.0) / 2, volume_map.VolumeToDbFS(0.25f), kEpsilon);
  EXPECT_NEAR((-10.0 - 0.0) / 2, volume_map.VolumeToDbFS(0.75f), kEpsilon);
}

TEST(VolumeMapTest, DbFSToVolumeInterpolates) {
  VolumeMap volume_map(std::make_unique<TestFileProvider>(kNewVolumeMap));
  EXPECT_NEAR(0.25f, volume_map.DbFSToVolume((-120.0 - 10.0) / 2), kEpsilon);
  EXPECT_NEAR(0.75f, volume_map.DbFSToVolume((-10.0 - 0.0) / 2), kEpsilon);
}

TEST(VolumeMapTest, LoadsNewMapWhenFileChanges) {
  auto provider = std::make_unique<TestFileProvider>("{}");
  TestFileProvider* provider_ptr = provider.get();
  VolumeMap volume_map(std::move(provider));

  provider_ptr->CallTuningChangedCallback(kNewVolumeMap);
  EXPECT_NEAR(-10.0f, volume_map.VolumeToDbFS(0.5), kEpsilon);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
