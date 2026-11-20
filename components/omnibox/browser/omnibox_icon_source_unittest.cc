// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_icon_source.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

namespace {

class MockOmniboxIconSource : public OmniboxIconSource {
 public:
  MOCK_METHOD(std::optional<SkBitmap>,
              RenderBitmap,
              (IconResourceIds id, int size_px, SkColor tint_color),
              (override));
  MOCK_METHOD(bool, IsMultiColor, (IconResourceIds id), (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

TEST(OmniboxIconSourceTest, SetSourceForTestingIsScoped) {
  OmniboxIconSource* default_source = &GetOmniboxIconSource();
  MockOmniboxIconSource mock;
  {
    auto reset = SetOmniboxIconSourceForTesting(&mock);
    EXPECT_EQ(&GetOmniboxIconSource(), &mock);
  }
  EXPECT_EQ(&GetOmniboxIconSource(), default_source);
}

}  // namespace

}  // namespace omnibox
