// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/volume_cache.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {
const int kMaxVolumeIndex = 10;

const float kTestVolumeTable[kMaxVolumeIndex + 1] = {
    -100.0f, -88.0f, -82.0f, -70.0f, -62.5f, -49.9f,
    -40.5f,  -30.0f, -28.2f, -10.0f, 0.0f};

}  // namespace

class VolumeCacheTest : protected SystemVolumeTableAccessApi,
                        public testing::Test {
 protected:
  VolumeCacheTest() : volume_cache_(AudioContentType::kMedia, this) {}

  ~VolumeCacheTest() override {}

  // SystemVolumeTableAccessApi implementation.
  // We use kTestVolumeTable for kMedia and just return -1.0f for other types.
  // That allows to test if the type is properly used in the c'tor.
  int GetMaxVolumeIndex(AudioContentType type) override {
    return (type == AudioContentType::kMedia) ? kMaxVolumeIndex : 2;
  }
  float VolumeToDbFS(AudioContentType type, float volume) override {
    if (type != AudioContentType::kMedia)
      return -1.0f;

    int idx_vol = static_cast<int>(std::round(volume * kMaxVolumeIndex));
    return kTestVolumeTable[idx_vol];
  }

  VolumeCache volume_cache_;
};

TEST_F(VolumeCacheTest, CachedValuesMatchesOriginalTable) {
  for (int i = 0; i <= kMaxVolumeIndex; i++) {
    float v = static_cast<float>(i) / kMaxVolumeIndex;
    EXPECT_FLOAT_EQ(kTestVolumeTable[i], volume_cache_.VolumeToDbFS(v));
    float db = kTestVolumeTable[i];
    EXPECT_FLOAT_EQ(v, volume_cache_.DbFSToVolume(db));
  }
}

TEST_F(VolumeCacheTest, BoundaryValues) {
  EXPECT_FLOAT_EQ(kTestVolumeTable[0], volume_cache_.VolumeToDbFS(-100.0f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[0], volume_cache_.VolumeToDbFS(-1.0f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[0], volume_cache_.VolumeToDbFS(-0.1f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[0], volume_cache_.VolumeToDbFS(0.0f));

  EXPECT_FLOAT_EQ(kTestVolumeTable[kMaxVolumeIndex],
                  volume_cache_.VolumeToDbFS(1.0f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[kMaxVolumeIndex],
                  volume_cache_.VolumeToDbFS(1.1f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[kMaxVolumeIndex],
                  volume_cache_.VolumeToDbFS(2.0f));
  EXPECT_FLOAT_EQ(kTestVolumeTable[kMaxVolumeIndex],
                  volume_cache_.VolumeToDbFS(100.0f));

  float min_db = kTestVolumeTable[0];
  EXPECT_FLOAT_EQ(0.0f, volume_cache_.DbFSToVolume(min_db - 100.0f));
  EXPECT_FLOAT_EQ(0.0f, volume_cache_.DbFSToVolume(min_db - 1.0f));
  EXPECT_FLOAT_EQ(0.0f, volume_cache_.DbFSToVolume(min_db - 0.1f));
  EXPECT_FLOAT_EQ(0.0f, volume_cache_.DbFSToVolume(min_db - 0.0f));

  float max_db = kTestVolumeTable[kMaxVolumeIndex];
  EXPECT_FLOAT_EQ(1.0f, volume_cache_.DbFSToVolume(max_db + 0.0f));
  EXPECT_FLOAT_EQ(1.0f, volume_cache_.DbFSToVolume(max_db + 0.1f));
  EXPECT_FLOAT_EQ(1.0f, volume_cache_.DbFSToVolume(max_db + 1.0f));
  EXPECT_FLOAT_EQ(1.0f, volume_cache_.DbFSToVolume(max_db + 100.0f));
}

TEST_F(VolumeCacheTest, Volume2DbFSInterpolatesCorrectly) {
  int i_low = 0, i_high = 1;
  for (; i_high <= kMaxVolumeIndex; ++i_high, ++i_low) {
    float v_low = static_cast<float>(i_low) / kMaxVolumeIndex;
    float v_high = static_cast<float>(i_high) / kMaxVolumeIndex;
    float db_low = kTestVolumeTable[i_low];
    float db_high = kTestVolumeTable[i_high];
    float m = (db_high - db_low) / (v_high - v_low);
    for (float v = v_low; v <= v_high; v += 0.1f) {
      float expected_db = db_low + m * (v - v_low);
      EXPECT_FLOAT_EQ(expected_db, volume_cache_.VolumeToDbFS(v));
    }
  }
}

TEST_F(VolumeCacheTest, DbFSToVolumeInterpolatesCorrectly) {
  int i_low = 0, i_high = 1;
  for (; i_high <= kMaxVolumeIndex; ++i_high, ++i_low) {
    float v_low = static_cast<float>(i_low) / kMaxVolumeIndex;
    float v_high = static_cast<float>(i_high) / kMaxVolumeIndex;
    float db_low = kTestVolumeTable[i_low];
    float db_high = kTestVolumeTable[i_high];
    float m = (v_high - v_low) / (db_high - db_low);
    for (float db = db_low; db <= db_high; db += 0.1f) {
      float expected_v = v_low + m * (db - db_low);
      EXPECT_FLOAT_EQ(expected_v, volume_cache_.DbFSToVolume(db));
    }
  }
}

TEST_F(VolumeCacheTest, CacheHonorsAudioContentType) {
  VolumeCache volume_cache(AudioContentType::kAlarm, this);
  EXPECT_FLOAT_EQ(-1.0f, volume_cache.VolumeToDbFS(0.0f));
  EXPECT_FLOAT_EQ(-1.0f, volume_cache.VolumeToDbFS(1.0f));
}

}  // namespace media
}  // namespace chromecast
