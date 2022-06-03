// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"

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
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(prefs::kAccessCodeCastEnabled,
                                                true);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessCodeCastDeviceDuration, 0);

  CastDialogAccessCodeCastButton button(views::Button::PressedCallback(),
                                        pref_service.get());
  EXPECT_EQ(u"Cast to a new device", button.title()->GetText());
}

TEST_F(CastDialogAccessCodeCastButtonTest, AddDeviceText) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(prefs::kAccessCodeCastEnabled,
                                                true);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessCodeCastDeviceDuration, 0);
  pref_service->SetManagedPref(prefs::kAccessCodeCastDeviceDuration,
                               std::make_unique<base::Value>(10));

  CastDialogAccessCodeCastButton button(views::Button::PressedCallback(),
                                        pref_service.get());
  EXPECT_EQ(u"Add new device", button.title()->GetText());
}

}  // namespace media_router
