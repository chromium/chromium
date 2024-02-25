// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_FOOTER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_FOOTER_H_

#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace global_media_controls {
namespace test {

class MockMediaItemUIFooter : public MediaItemUIFooter {
  METADATA_HEADER(MockMediaItemUIFooter, MediaItemUIFooter)

 public:
  MockMediaItemUIFooter();
  MockMediaItemUIFooter(const MockMediaItemUIFooter&) = delete;
  MockMediaItemUIFooter& operator=(const MockMediaItemUIFooter&) = delete;
  ~MockMediaItemUIFooter() override;

  // MediaItemUIFooter:
  MOCK_METHOD(void, OnColorsChanged, (SkColor, SkColor));

  MOCK_METHOD(void, Die, ());
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_FOOTER_H_
