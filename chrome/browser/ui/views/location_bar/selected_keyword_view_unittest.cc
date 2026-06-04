// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/widget.h"

namespace {

class SelectedKeywordViewAccessibilityTest
    : public ChromeViewsTestBase,
      public IconLabelBubbleView::Delegate {
 public:
  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return SK_ColorBLACK;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return SK_ColorWHITE;
  }

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    template_url_service_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    gfx::FontList font_list;
    view_ = widget_->SetContentsView(std::make_unique<SelectedKeywordView>(
        this, profile_.get(),
        /*omnibox_controller=*/nullptr, font_list));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    template_url_service_util_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  SelectedKeywordView* view() { return view_; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> template_url_service_util_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<SelectedKeywordView> view_ = nullptr;
};

TEST_F(SelectedKeywordViewAccessibilityTest,
       LiveRegionAttributesSetOnConstruction) {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);

  EXPECT_EQ("polite",
            data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus));
  EXPECT_EQ("polite", data.GetStringAttribute(
                          ax::mojom::StringAttribute::kContainerLiveStatus));
  EXPECT_EQ("additions text",
            data.GetStringAttribute(ax::mojom::StringAttribute::kLiveRelevant));
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic));
}

TEST_F(SelectedKeywordViewAccessibilityTest,
       ChildLabelInheritsContainerLiveStatus) {
  views::Label* label = view()->label();
  ASSERT_NE(label, nullptr);

  ui::AXNodeData label_data;
  label->GetViewAccessibility().GetAccessibleNodeData(&label_data);

  EXPECT_EQ("polite", label_data.GetStringAttribute(
                          ax::mojom::StringAttribute::kContainerLiveStatus));
  EXPECT_FALSE(
      label_data.HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus));
}

TEST_F(SelectedKeywordViewAccessibilityTest,
       LiveRegionChangedFiredOnLabelChange) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());

  view()->SetLabel(u"Search example.com");

  EXPECT_GE(counter.GetCount(ax::mojom::Event::kLiveRegionChanged, view()), 1);
}

}  // namespace
