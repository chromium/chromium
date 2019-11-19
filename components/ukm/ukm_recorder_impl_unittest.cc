// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include "base/bind.h"
#include "base/metrics/ukm_source_id.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "url/gurl.h"

namespace ukm {

using TestEvent1 = builders::PageLoad;

TEST(UkmRecorderImplTest, IsSampledIn) {
  UkmRecorderImpl impl;

  for (int i = 0; i < 100; ++i) {
    // These are constant regardless of the seed, source, and event.
    EXPECT_FALSE(impl.IsSampledIn(-i, i, 0));
    EXPECT_TRUE(impl.IsSampledIn(-i, i, 1));
  }

  // These depend on the source, event, and initial seed. There's no real
  // predictability here but should see roughly 50% true and 50% false with
  // no obvious correlation and the same for every run of the test.
  impl.SetSamplingSeedForTesting(123);
  EXPECT_FALSE(impl.IsSampledIn(1, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(1, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(456);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(789);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));
}

TEST(UkmRecorderImplTest, PurgeExtensionRecordings) {
  TestUkmRecorder recorder;
  // Enable extension sync.
  recorder.SetIsWebstoreExtensionCallback(
      base::BindRepeating([](base::StringPiece) { return true; }));

  // Record some sources and events.
  SourceId id1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id1, GURL("https://www.google.ca"));
  SourceId id2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id2, GURL("chrome-extension://abc/manifest.json"));
  SourceId id3 = ConvertToSourceId(3, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id3, GURL("http://www.wikipedia.org"));
  SourceId id4 = ConvertToSourceId(4, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id4, GURL("chrome-extension://abc/index.html"));

  TestEvent1(id1).Record(&recorder);
  TestEvent1(id2).Record(&recorder);

  // All sources and events have been recorded.
  EXPECT_TRUE(recorder.extensions_enabled_);
  EXPECT_TRUE(recorder.recording_is_continuous_);
  EXPECT_EQ(4U, recorder.sources().size());
  EXPECT_EQ(2U, recorder.entries().size());

  recorder.PurgeExtensionRecordings();

  // Recorded sources of extension scheme and related events have been cleared.
  EXPECT_EQ(2U, recorder.sources().size());
  EXPECT_EQ(1U, recorder.sources().count(id1));
  EXPECT_EQ(0U, recorder.sources().count(id2));
  EXPECT_EQ(1U, recorder.sources().count(id3));
  EXPECT_EQ(0U, recorder.sources().count(id4));

  EXPECT_FALSE(recorder.recording_is_continuous_);
  EXPECT_EQ(1U, recorder.entries().size());
  EXPECT_EQ(id1, recorder.entries()[0]->source_id);

  // Recording is disabled for extensions, thus new extension URL will not be
  // recorded.
  recorder.EnableRecording(/* extensions = */ false);
  recorder.UpdateSourceURL(id4, GURL("chrome-extension://abc/index.html"));
  EXPECT_FALSE(recorder.extensions_enabled_);
  EXPECT_EQ(2U, recorder.sources().size());
}

}  // namespace ukm
