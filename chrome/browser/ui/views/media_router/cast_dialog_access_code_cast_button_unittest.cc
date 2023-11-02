// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/views/controls/styled_label.h"

namespace media_router {

class CastDialogAccessCodeCastButtonTest : public ChromeViewsTestBase {
 public:
  CastDialogAccessCodeCastButtonTest() = default;

  CastDialogAccessCodeCastButtonTest(
      const CastDialogAccessCodeCastButtonTest&) = delete;
  CastDialogAccessCodeCastButtonTest& operator=(
      const CastDialogAccessCodeCastButtonTest&) = delete;

  ~CastDialogAccessCodeCastButtonTest() override = default;
};

TEST_F(CastDialogAccessCodeCastButtonTest, DefaultText) {
  CastDialogAccessCodeCastButton button((views::Button::PressedCallback()));
  EXPECT_EQ(u"Connect with a code", button.title()->GetText());
}

}  // namespace media_router
