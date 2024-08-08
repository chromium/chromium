// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

// Handles requests for prefs::kCookieControlsMode retrival/update.
class CookieControlsServiceObserver : public CookieControlsService::Observer {
 public:
  explicit CookieControlsServiceObserver(Profile* profile) {
    service_ = CookieControlsServiceFactory::GetForProfile(profile);
    service_->AddObserver(this);
    checked_ = false;
  }

  CookieControlsServiceObserver(const CookieControlsServiceObserver&) = delete;
  CookieControlsServiceObserver& operator=(
      const CookieControlsServiceObserver&) = delete;

  ~CookieControlsServiceObserver() override = default;

  CookieControlsService* GetService() { return service_; }
  void SetChecked(bool checked) { checked_ = checked; }
  bool GetChecked() { return checked_; }

  // CookieControlsService::Observer
  void OnThirdPartyCookieBlockingPrefChanged() override {
    SetChecked(service_->GetToggleCheckedValue());
  }

 private:
  raw_ptr<CookieControlsService, DanglingUntriaged> service_;
  bool checked_;
};

class CookieControlsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  std::unique_ptr<CookieControlsServiceObserver> observer_;

};

TEST_F(CookieControlsServiceTest, HandleCookieControlsToggleChanged) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  observer_ = std::make_unique<CookieControlsServiceObserver>(otr_profile);
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      otr_profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  // Set toggle value to false
  observer_->SetChecked(true);
  observer_->GetService()->HandleCookieControlsToggleChanged(false);
  EXPECT_EQ(static_cast<int>(content_settings::CookieControlsMode::kOff),
            otr_profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));
  EXPECT_EQ(observer_->GetChecked(), false);

  // Set toggle value to true
  observer_->GetService()->HandleCookieControlsToggleChanged(true);
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      otr_profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  EXPECT_EQ(observer_->GetChecked(), true);
}
