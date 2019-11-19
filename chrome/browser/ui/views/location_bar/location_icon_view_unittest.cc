// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "ui/views/widget/widget.h"

namespace {

class TestLocationIconDelegate : public LocationIconView::Delegate {
 public:
  explicit TestLocationIconDelegate(LocationBarModel* location_bar_model)
      : location_bar_model_(location_bar_model) {}

  content::WebContents* GetWebContents() override { return nullptr; }

  bool IsEditingOrEmpty() const override { return is_editing_or_empty_; }
  void set_is_editing_or_empty(bool is_editing_or_empty) {
    is_editing_or_empty_ = is_editing_or_empty;
  }

  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override {
    return SK_ColorWHITE;
  }

  bool ShowPageInfoDialog() override { return false; }

  // Gets the LocationBarModel.
  const LocationBarModel* GetLocationBarModel() const override {
    return location_bar_model_;
  }

  // Gets an icon for the location bar icon chip.
  gfx::ImageSkia GetLocationIcon(
      IconFetchedCallback on_icon_fetched) const override {
    return gfx::ImageSkia();
  }

  // Gets the color to use for icon ink highlights.
  SkColor GetLocationIconInkDropColor() const override { return SK_ColorBLACK; }

 private:
  LocationBarModel* location_bar_model_;
  bool is_editing_or_empty_ = false;
};

}  // namespace

class LocationIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    gfx::FontList font_list;

    CreateWidget();

    location_bar_model_ = std::make_unique<TestLocationBarModel>();
    delegate_ =
        std::make_unique<TestLocationIconDelegate>(location_bar_model());

    view_ = new LocationIconView(font_list, delegate());
    view_->SetBoundsRect(gfx::Rect(0, 0, 24, 24));
    widget_->SetContentsView(view_);

    widget_->Show();
  }

  void TearDown() override {
    if (widget_ && !widget_->IsClosed())
      widget_->Close();

    ChromeViewsTestBase::TearDown();
  }

  TestLocationBarModel* location_bar_model() {
    return location_bar_model_.get();
  }

  void SetSecurityLevel(security_state::SecurityLevel level) {
    location_bar_model()->set_security_level(level);

    base::string16 secure_display_text = base::string16();
    if (level == security_state::SecurityLevel::DANGEROUS ||
        level == security_state::SecurityLevel::WARNING)
      secure_display_text = base::ASCIIToUTF16("Insecure");

    location_bar_model()->set_secure_display_text(secure_display_text);
  }

  TestLocationIconDelegate* delegate() { return delegate_.get(); }
  LocationIconView* view() { return view_; }

 private:
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationIconDelegate> delegate_;
  LocationIconView* view_;
  views::Widget* widget_ = nullptr;

  void CreateWidget() {
    DCHECK(!widget_);

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
  }
};

TEST_F(LocationIconViewTest, ShouldNotAnimateWhenSuppressingAnimations) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/true);
  // When we change tab, suppress animations is true.
  EXPECT_FALSE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldAnimateTextWhenWarning) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::WARNING);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_TRUE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldAnimateTextWhenDangerous) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_TRUE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldNotAnimateWarningToDangerous) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::WARNING);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_FALSE(view()->is_animating_label());
}

// Whenever InkDropMode is set a new InkDrop is created, which will reset any
// animations on the drop, so we should only set the InkDropMode when it has
// actually changed.
TEST_F(LocationIconViewTest, ShouldNotRecreateInkDropNeedlessly) {
  delegate()->set_is_editing_or_empty(false);
  view()->Update(false);

  const views::InkDrop* drop = view()->get_ink_drop_for_testing();
  view()->Update(/*suppress_animations=*/false);

  // The InkDropMode has not changed (is ON), so our InkDrop should remain the
  // same.
  EXPECT_EQ(drop, view()->get_ink_drop_for_testing());

  delegate()->set_is_editing_or_empty(true);
  view()->Update(/*suppress_animations=*/false);

  // The InkDropMode has changed (ON --> OFF), so a new InkDrop will have been
  // created.
  EXPECT_NE(drop, view()->get_ink_drop_for_testing());

  drop = view()->get_ink_drop_for_testing();
  view()->Update(/*suppress_animations=*/false);

  // The InkDropMode has not changed (is OFF), so the InkDrop should remain the
  // same.
  EXPECT_EQ(drop, view()->get_ink_drop_for_testing());

  delegate()->set_is_editing_or_empty(false);
  view()->Update(/*suppress_animations=*/false);

  // The InkDrop mode has changed (OFF --> ON), so a new InkDrop will have been
  // created.
  EXPECT_NE(drop, view()->get_ink_drop_for_testing());
}
