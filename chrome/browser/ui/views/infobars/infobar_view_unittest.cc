// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

class TestInfoBarDelegateWithIcon : public infobars::InfoBarDelegate {
 public:
  TestInfoBarDelegateWithIcon() = default;
  ~TestInfoBarDelegateWithIcon() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return TEST_INFOBAR;
  }

  const gfx::VectorIcon& GetVectorIcon() const override {
    return vector_icons::kWarningIcon;
  }
};

class TestInfoBarViewWithLabelAndIcon : public InfoBarView {
 public:
  explicit TestInfoBarViewWithLabelAndIcon(
      std::unique_ptr<infobars::InfoBarDelegate> delegate)
      : InfoBarView(std::move(delegate)) {
    test_label_ = AddContentChildView(CreateLabel(u"Test Label"));
  }

  views::Label* test_label() { return test_label_; }
  views::ImageView* icon() {
    for (views::View* child : children()) {
      if (views::IsViewClass<views::ImageView>(child)) {
        return static_cast<views::ImageView*>(child);
      }
    }
    return nullptr;
  }
  int GetPublicEndX() const { return GetEndX(); }

 protected:
  int GetContentMinimumWidth() const override {
    return test_label_->GetPreferredSize().width();
  }

  int GetContentPreferredWidth() const override {
    return test_label_->GetPreferredSize().width();
  }

 private:
  raw_ptr<views::Label> test_label_ = nullptr;
};

}  // namespace

class InfoBarViewUnitTest : public views::ViewsTestBase {
 public:
  InfoBarViewUnitTest() {
    feature_list_.InitAndEnableFeature(features::kInfobarRefresh);
  }

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    layout_provider_ = std::make_unique<ChromeLayoutProvider>();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeLayoutProvider> layout_provider_;
};

TEST_F(InfoBarViewUnitTest, CenteredLayout) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  // The widget will take ownership of this view.
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  views::ImageView* icon = infobar_view->icon();
  ASSERT_NE(nullptr, icon);
  views::Label* label = infobar_view->test_label();
  const int spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int content_width = icon->GetPreferredSize().width() + spacing / 2 +
                            label->GetPreferredSize().width();
  const int available_width = infobar_view->GetPublicEndX();
  const int expected_start_x = (available_width - content_width) / 2;

  EXPECT_EQ(expected_start_x, icon->x());
  widget->CloseNow();
  widget.reset();
}
