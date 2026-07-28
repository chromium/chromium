// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/view_utils.h"

class ContentSettingBubbleContentsTest : public ChromeViewsTestBase {
 public:
  Profile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

class TestContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  explicit TestContentSettingBubbleModel(content::Page& page)
      : ContentSettingBubbleModel(nullptr, page) {
    AddListItem(
        ListItem(nullptr, std::u16string(), std::u16string(), false, false, 0));
  }
};

class TestContentSettingBubbleContents : public ContentSettingBubbleContents {
  METADATA_HEADER(TestContentSettingBubbleContents,
                  ContentSettingBubbleContents)

 public:
  TestContentSettingBubbleContents(
      std::unique_ptr<ContentSettingBubbleModel> bubble_model,
      content::WebContents* web_contents,
      gfx::NativeView parent_window)
      : ContentSettingBubbleContents(std::move(bubble_model),
                                     web_contents,
                                     views::BubbleAnchor(),
                                     views::BubbleBorder::TOP_LEFT) {
    set_parent_window(parent_window);
  }

  // ContentSettingBubbleContents:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
  }
};

BEGIN_METADATA(TestContentSettingBubbleContents)
END_METADATA

// Regression test for http://crbug.com/40673356 .
TEST_F(ContentSettingBubbleContentsTest, NullDeref) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<views::Widget> parent_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent_widget->Show();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Should not crash.
  std::unique_ptr<views::Widget> widget(
      views::BubbleDialogDelegateView::CreateBubble(
          std::make_unique<TestContentSettingBubbleContents>(
              std::make_unique<TestContentSettingBubbleModel>(
                  web_contents->GetPrimaryPage()),
              web_contents.get(), parent_widget->GetNativeView())));
  widget->Show();
}

class TestContentSettingUrlBubbleModel : public ContentSettingBubbleModel {
 public:
  TestContentSettingUrlBubbleModel(content::Page& page, const GURL& url)
      : ContentSettingBubbleModel(nullptr, page) {
    ListItem item(nullptr, FormatUrlWithBullet(url), std::u16string(),
                  /*has_link=*/true, /*has_blocked_badge=*/false, 0);
    item.url = url;
    AddListItem(item);
  }
};

namespace {
void FindLinks(views::View* view, std::vector<views::Link*>* links) {
  if (views::IsViewClass<views::Link>(view)) {
    links->push_back(static_cast<views::Link*>(view));
  }
  for (views::View* child : view->children()) {
    FindLinks(child, links);
  }
}
}  // namespace

TEST_F(ContentSettingBubbleContentsTest, BulletUrlLinkElision) {
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<views::Widget> parent_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent_widget->Show();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  const GURL url(
      "https://google.com.evildomain-mybanklongdomainname.com/some/long/path");

  auto bubble = std::make_unique<TestContentSettingBubbleContents>(
      std::make_unique<TestContentSettingUrlBubbleModel>(
          web_contents->GetPrimaryPage(), url),
      web_contents.get(), parent_widget->GetNativeView());

  std::unique_ptr<views::Widget> widget(
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble)));
  widget->Show();

  std::vector<views::Link*> links;
  FindLinks(widget->GetContentsView(), &links);
  ASSERT_EQ(1u, links.size());
  views::Link* link = links[0];

  // Verify LTR directionality is set for URL display.
  EXPECT_EQ(gfx::DirectionalityMode::DIRECTIONALITY_AS_URL,
            link->GetDirectionalityMode());

  // Test Case 1: Large bounds (no elision).
  link->SetBounds(0, 0, 1000, 20);
  std::u16string full_expected =
      ContentSettingBubbleModel::FormatUrlWithBullet(url);
  EXPECT_EQ(full_expected, link->GetText());
  EXPECT_EQ(full_expected, link->GetTooltipText());

  // Test Case 2: Narrower width. Registrable domain must stay intact.
  std::u16string bullet_prefix =
      ContentSettingBubbleModel::FormatTitleWithBullet(std::u16string());
  std::u16string domain_only = u"evildomain-mybanklongdomainname.com";
  float domain_width =
      gfx::GetStringWidthF(bullet_prefix + domain_only, link->font_list());

  link->SetBounds(0, 0, domain_width + 40, 20);
  std::u16string elided_text(link->GetText());

  EXPECT_TRUE(elided_text.find(domain_only) != std::u16string::npos)
      << "Elided text '" << base::UTF16ToUTF8(elided_text)
      << "' does not contain registrable domain '"
      << base::UTF16ToUTF8(domain_only) << "'";
  EXPECT_EQ(full_expected, link->GetTooltipText());

  // Test Case 3: Space smaller than domain width.
  link->SetBounds(0, 0, domain_width - 30, 20);
  std::u16string narrow_text(link->GetText());

  // It should NOT show "mybanklongdomainname.com" without the "evildomain-"
  // prefix.
  EXPECT_TRUE(narrow_text.find(u"mybanklongdomainname.com") ==
              std::u16string::npos);
  EXPECT_TRUE(narrow_text.find(u"evildomain-") != std::u16string::npos)
      << "narrow_text: '" << base::UTF16ToUTF8(narrow_text) << "'";
  EXPECT_EQ(full_expected, link->GetTooltipText());

  widget->CloseNow();
}

TEST_F(ContentSettingBubbleContentsTest, PrimaryPageWillBeDeactivated) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<views::Widget> parent_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent_widget->Show();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Navigate to initial page.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));

  auto bubble_contents = std::make_unique<TestContentSettingBubbleContents>(
      std::make_unique<TestContentSettingBubbleModel>(
          web_contents->GetPrimaryPage()),
      web_contents.get(), parent_widget->GetNativeView());
  TestContentSettingBubbleContents* bubble_contents_ptr = bubble_contents.get();

  std::unique_ptr<views::Widget> widget(
      views::BubbleDialogDelegateView::CreateBubble(
          std::move(bubble_contents)));
  widget->Show();

  EXPECT_FALSE(widget->IsClosed());
  EXPECT_NE(nullptr, bubble_contents_ptr->bubble_model_for_test());

  // Navigate again to trigger PrimaryPageWillBeDeactivated.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example2.com"));

  // The model should be reset synchronously.
  EXPECT_EQ(nullptr, bubble_contents_ptr->bubble_model_for_test());

  // The widget should be closed.
  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(ContentSettingBubbleContentsTest, WebContentsDestroyed) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<views::Widget> parent_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  parent_widget->Show();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Navigate to initial page.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));

  auto bubble_contents = std::make_unique<TestContentSettingBubbleContents>(
      std::make_unique<TestContentSettingBubbleModel>(
          web_contents->GetPrimaryPage()),
      web_contents.get(), parent_widget->GetNativeView());
  TestContentSettingBubbleContents* bubble_contents_ptr = bubble_contents.get();

  std::unique_ptr<views::Widget> widget(
      views::BubbleDialogDelegateView::CreateBubble(
          std::move(bubble_contents)));
  widget->Show();

  EXPECT_FALSE(widget->IsClosed());
  EXPECT_NE(nullptr, bubble_contents_ptr->bubble_model_for_test());

  // Destroy WebContents.
  web_contents.reset();

  // The model should be reset synchronously.
  EXPECT_EQ(nullptr, bubble_contents_ptr->bubble_model_for_test());

  // The widget should be closed.
  EXPECT_TRUE(widget->IsClosed());
}
