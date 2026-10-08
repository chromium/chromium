// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_web_contents_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget.h"

namespace {

class TestLocationIconDelegate : public IconLabelBubbleView::Delegate,
                                 public LocationIconView::Delegate {
 public:
  explicit TestLocationIconDelegate(LocationBarModel* location_bar_model)
      : location_bar_model_(location_bar_model) {}
  virtual ~TestLocationIconDelegate() = default;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return SK_ColorBLACK;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return SK_ColorWHITE;
  }

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override { return web_contents_; }
  bool IsEditingOrEmpty() const override { return is_editing_or_empty_; }
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override {
    return GetIconLabelBubbleSurroundingForegroundColor();
  }
  bool ShowPageInfoDialog() override { return false; }
  const LocationBarModel* GetLocationBarModel() const override {
    return location_bar_model_;
  }
  ui::ImageModel GetLocationIcon(IconFetchedCallback on_icon_fetched) override {
    return ui::ImageModel();
  }

  void set_is_editing_or_empty(bool is_editing_or_empty) {
    is_editing_or_empty_ = is_editing_or_empty;
  }
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  raw_ptr<LocationBarModel> location_bar_model_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  bool is_editing_or_empty_ = false;
};

class TestLocationIconView : public LocationIconView {
 public:
  using LocationIconView::IsTriggerableEvent;
  using LocationIconView::LocationIconView;
};

class TestPageInfoBubbleView : public PageInfoBubbleViewBase {
 public:
  TestPageInfoBubbleView(views::View* anchor_view,
                         content::WebContents* web_contents)
      : PageInfoBubbleViewBase(views::BubbleAnchor(anchor_view),
                               gfx::Rect(),
                               gfx::NativeView(),
                               BUBBLE_PAGE_INFO,
                               web_contents) {
    views::BubbleDialogDelegateView::CreateBubble(this);
  }
  ~TestPageInfoBubbleView() override = default;
};

}  // namespace

class LocationIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    gfx::FontList font_list;

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    location_bar_model_ = std::make_unique<TestLocationBarModel>();
    delegate_ =
        std::make_unique<TestLocationIconDelegate>(location_bar_model());

    auto view = std::make_unique<TestLocationIconView>(font_list, delegate(),
                                                       delegate());
    view->SetBoundsRect(gfx::Rect(0, 0, 24, 24));
    view_ = widget_->SetContentsView(std::move(view));

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestLocationBarModel* location_bar_model() {
    return location_bar_model_.get();
  }

  void SetSecurityLevel(security_state::SecurityLevel level) {
    location_bar_model()->set_security_level(level);

    std::u16string secure_display_text = std::u16string();
    if (level == security_state::SecurityLevel::DANGEROUS ||
        level == security_state::SecurityLevel::WARNING) {
      secure_display_text = u"Insecure";
    }

    location_bar_model()->set_secure_display_text(secure_display_text);
  }

  TestLocationIconDelegate* delegate() { return delegate_.get(); }
  LocationIconView* view() { return view_; }
  TestLocationIconView* test_view() {
    return static_cast<TestLocationIconView*>(view_);
  }

 private:
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationIconDelegate> delegate_;
  raw_ptr<LocationIconView, DanglingUntriaged> view_;
  std::unique_ptr<views::Widget> widget_;
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

TEST_F(LocationIconViewTest, IconViewAccessibleNameAndRole) {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kPopUpButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kPopUpButton);

  delegate()->set_is_editing_or_empty(true);
  view()->Update(/*suppress_animations=*/true);
  data = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_ACC_SEARCH_ICON));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ACC_SEARCH_ICON));
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kImage);
  EXPECT_EQ(data.role, ax::mojom::Role::kImage);

  delegate()->set_is_editing_or_empty(false);
  SetSecurityLevel(security_state::SecurityLevel::WARNING);
  view()->Update(/*suppress_animations=*/true);
  data = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedName(), u"Insecure");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Insecure");
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kPopUpButton);
  EXPECT_EQ(data.role, ax::mojom::Role::kPopUpButton);
}

TEST_F(LocationIconViewTest,
       CrossWebContentsBubbleShowingDoesNotSuppressClicks) {
  TestingProfile profile;
  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* contents_a =
      web_contents_factory.CreateWebContents(&profile);
  content::WebContents* contents_b =
      web_contents_factory.CreateWebContents(&profile);

  // Set web_contents for view() (window A).
  delegate()->set_web_contents(contents_a);

  // Create a second LocationIconView in a second widget (window B).
  auto model_b = std::make_unique<TestLocationBarModel>();
  auto delegate_b = std::make_unique<TestLocationIconDelegate>(model_b.get());
  delegate_b->set_web_contents(contents_b);

  auto widget_b =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  gfx::FontList font_list;
  auto icon_b_unique = std::make_unique<TestLocationIconView>(
      font_list, delegate_b.get(), delegate_b.get());
  icon_b_unique->SetBoundsRect(gfx::Rect(0, 0, 24, 24));
  TestLocationIconView* icon_b =
      widget_b->SetContentsView(std::move(icon_b_unique));
  widget_b->Show();

  // Initially, neither icon has a bubble showing.
  EXPECT_FALSE(view()->IsBubbleShowing());
  EXPECT_FALSE(icon_b->IsBubbleShowing());

  // Show a PageInfo bubble on contents_a / window A.
  auto* bubble_a = new TestPageInfoBubbleView(view(), contents_a);
  bubble_a->GetWidget()->Show();

  // Window A's icon correctly sees its bubble showing.
  EXPECT_TRUE(view()->IsBubbleShowing());

  // Window A's icon suppresses clicks when its bubble is open (toggle
  // behavior).
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  test_view()->OnMousePressed(press_event);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), base::TimeTicks::Now(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_FALSE(test_view()->IsTriggerableEvent(release_event));

  // Window B's icon must NOT see the bubble from Window A showing.
  // Before the fix for bug 556215096, this returns true because of the global
  // bubble state, which suppresses clicks on Window B.
  EXPECT_FALSE(icon_b->IsBubbleShowing());

  // Simulate mouse press and release on icon_b to verify clicks are accepted.
  icon_b->OnMousePressed(press_event);
  EXPECT_TRUE(icon_b->IsTriggerableEvent(release_event));

  bubble_a->GetWidget()->CloseNow();
  widget_b.reset();
  delegate()->set_web_contents(nullptr);
}
