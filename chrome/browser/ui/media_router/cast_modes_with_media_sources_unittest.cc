// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_modes_with_media_sources.h"

#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/test/test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_router {

TEST(MediaRouterCastModesWithMediaSourcesTest, AddAndRemoveSources) {
  const MediaSource presentationSource1(MediaSource::ForPresentationUrl(
      GURL("http://www.example.com/presentation.html")));
  const MediaSource presentationSource2(MediaSource::ForPresentationUrl(
      GURL("http://www.example.net/presentation.html")));
  const MediaSource tabSourceA(MediaSource::ForTab(123));
  const CastModeSet castModeSetEmpty;
  const CastModeSet castModeSetPresentation({MediaCastMode::PRESENTATION});
  const CastModeSet castModeSetTab({MediaCastMode::TAB_MIRROR});
  const CastModeSet castModeSetPresentationAndTab(
      {MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR});

  CastModesWithMediaSources sources(CreateDialSink("sinkId", "name"));
  EXPECT_TRUE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetEmpty);

  // After the below addition, |sources| should contain:
  // [Presentation: 1]
  sources.AddSource(MediaCastMode::PRESENTATION, presentationSource1);
  EXPECT_TRUE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource2));
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::TAB_MIRROR, presentationSource1));
  EXPECT_FALSE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetPresentation);

  // Trying to remove non-existing sources should be no-op.
  sources.RemoveSource(MediaCastMode::PRESENTATION, presentationSource2);
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, presentationSource1);
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_TRUE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_EQ(sources.GetCastModes(), castModeSetPresentation);

  // [Presentation: 1; Tab: A]
  sources.AddSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_TRUE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_TRUE(sources.HasSource(MediaCastMode::TAB_MIRROR, tabSourceA));
  EXPECT_EQ(sources.GetCastModes(), castModeSetPresentationAndTab);

  // [Presentation: 1,2; Tab: A]
  sources.AddSource(MediaCastMode::PRESENTATION, presentationSource2);
  EXPECT_TRUE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource2));
  EXPECT_EQ(sources.GetCastModes(), castModeSetPresentationAndTab);

  // [Presentation: 2; Tab: A]
  sources.RemoveSource(MediaCastMode::PRESENTATION, presentationSource1);
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_TRUE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource2));
  EXPECT_EQ(sources.GetCastModes(), castModeSetPresentationAndTab);

  // [Tab: A]
  sources.RemoveSource(MediaCastMode::PRESENTATION, presentationSource2);
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_FALSE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetTab);

  // []
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource1));
  EXPECT_FALSE(
      sources.HasSource(MediaCastMode::PRESENTATION, presentationSource2));
  EXPECT_FALSE(sources.HasSource(MediaCastMode::TAB_MIRROR, tabSourceA));
  EXPECT_TRUE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetEmpty);
}

}  // namespace media_router
