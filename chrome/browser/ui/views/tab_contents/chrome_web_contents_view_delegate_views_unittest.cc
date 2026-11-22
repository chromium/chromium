// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/test/test_web_contents_factory.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace {

// Lets tests keep the widget inactive after `RequestFocus()`.
class TestFocusableView : public views::View {
  METADATA_HEADER(TestFocusableView, views::View)

 public:
  TestFocusableView() { SetFocusBehavior(FocusBehavior::ALWAYS); }

  void set_suppress_activation_on_focus(bool suppress) {
    suppress_activation_on_focus_ = suppress;
  }

  // views::View:
  void RequestFocus() override {
    if (GetWidget() && !GetWidget()->IsActive()) {
      GetWidget()->GetFocusManager()->SetStoredFocusView(this);
      if (!suppress_activation_on_focus_) {
        GetWidget()->Activate();
      }
      return;
    }
    views::View::RequestFocus();
  }

 private:
  bool suppress_activation_on_focus_ = false;
};

BEGIN_METADATA(TestFocusableView)
END_METADATA

// Simulates `BrowserView::OnWidgetActivationChanged()` calling `RestoreFocus()`
// when `restore_focus_on_activation_` is set.
class ActivationRestoreFocusObserver : public views::WidgetObserver {
 public:
  ActivationRestoreFocusObserver(views::Widget* widget,
                                 ChromeWebContentsViewFocusHelper* focus_helper)
      : focus_helper_(focus_helper) {
    observation_.Observe(widget);
  }

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (active && restore_focus_on_activation_) {
      restore_focus_on_activation_ = false;
      activation_restore_result_ = focus_helper_->RestoreFocus();
    }
  }

  bool activation_restore_result() const { return activation_restore_result_; }

 private:
  raw_ptr<ChromeWebContentsViewFocusHelper> focus_helper_;
  bool restore_focus_on_activation_ = true;
  bool activation_restore_result_ = false;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

class ChromeWebContentsViewFocusHelperTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    ChromeWebContentsViewFocusHelper::CreateForWebContents(web_contents_);

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* container = widget_->SetContentsView(std::make_unique<views::View>());
    web_view_ = container->AddChildView(
        std::make_unique<views::WebView>(profile_.get()));
    web_view_->SetWebContents(web_contents_);
    focusable_view_ =
        container->AddChildView(std::make_unique<TestFocusableView>());
    widget_->Show();
  }

  void TearDown() override {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
    focusable_view_ = nullptr;
    widget_.reset();
    web_contents_factory_.DestroyWebContents(web_contents_.ExtractAsDangling());
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  ChromeWebContentsViewFocusHelper* focus_helper() {
    return ChromeWebContentsViewFocusHelper::FromWebContents(web_contents_);
  }

  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<TestFocusableView> focusable_view_ = nullptr;
};

}  // namespace

using ChromeWebContentsViewDelegateViewsTest = ChromeRenderViewHostTestHarness;

// Test that makes sure ShowContextMenu does not crash if web_contents() does
// not have a focused frame.
TEST_F(ChromeWebContentsViewDelegateViewsTest, ContextMenuNoFocusedFrame) {
  std::unique_ptr<content::WebContentsViewDelegate> delegate_view =
      CreateWebContentsViewDelegate(web_contents());
  EXPECT_FALSE(web_contents()->GetFocusedFrame());
  const content::ContextMenuParams params;
  delegate_view->ShowContextMenu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
}

namespace {

using MenuFuture =
    base::test::TestFuture<std::unique_ptr<RenderViewContextMenuBase>>;
using blink::ContextMenuDataEditFlags;

class DelayedClipboard : public ui::TestClipboard {
 public:
  void ReadAvailableTypes(
      ui::ClipboardBuffer buffer,
      const std::optional<ui::DataTransferEndpoint>& data_dst,
      ReadAvailableTypesCallback callback) const override {
    types_callbacks.push_back(std::move(callback));
  }

  void GetAllAvailableFormats(
      ui::ClipboardBuffer buffer,
      const std::optional<ui::DataTransferEndpoint>& data_dst,
      base::OnceCallback<void(base::flat_set<ui::ClipboardFormatType>)>
          callback) const override {
    formats_callbacks.push_back(std::move(callback));
  }

  mutable std::vector<ReadAvailableTypesCallback> types_callbacks;
  mutable std::vector<
      base::OnceCallback<void(base::flat_set<ui::ClipboardFormatType>)>>
      formats_callbacks;
};

class ChromeWebContentsViewDelegateViewsClipboardTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ui::Clipboard::SetClipboardForCurrentThread(
        std::make_unique<DelayedClipboard>());
    NavigateAndCommit(GURL("https://example.test/"));
    delegate_view_ = CreateWebContentsViewDelegate(web_contents());
    ASSERT_TRUE(ContextMenuDelegate::FromWebContents(web_contents()));
  }

  void TearDown() override {
    delegate_view_.reset();
    ui::Clipboard::DestroyClipboardForCurrentThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  DelayedClipboard* clipboard() {
    return static_cast<DelayedClipboard*>(ui::Clipboard::GetForCurrentThread());
  }

  ContextMenuDelegate& delegate() {
    return *ContextMenuDelegate::FromWebContents(web_contents());
  }

 private:
  std::unique_ptr<content::WebContentsViewDelegate> delegate_view_;

#if BUILDFLAG(IS_CHROMEOS)
  testing::NiceMock<ash::MockClipboardHistoryController>
      clipboard_history_controller_;
#endif
};

TEST_F(ChromeWebContentsViewDelegateViewsClipboardTest,
       SkipsClipboardReadsWithoutPasteFlag) {
  for (bool is_editable : {false, true}) {
    content::ContextMenuParams params;
    params.is_editable = is_editable;
    MenuFuture future;
    delegate().BuildMenuAsync(*main_rfh(), params, future.GetCallback());

    ASSERT_TRUE(future.IsReady());
    ASSERT_TRUE(future.Get());
    if (is_editable) {
      const auto& model = future.Get()->menu_model();
      auto paste = model.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE);
      ASSERT_TRUE(paste);
      EXPECT_FALSE(model.IsEnabledAt(*paste));
      auto paste_and_match_style =
          model.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
      ASSERT_TRUE(paste_and_match_style);
      EXPECT_FALSE(model.IsEnabledAt(*paste_and_match_style));
    }
  }
  EXPECT_TRUE(clipboard()->types_callbacks.empty());
  EXPECT_TRUE(clipboard()->formats_callbacks.empty());
}

TEST_F(ChromeWebContentsViewDelegateViewsClipboardTest,
       PreservesSearchPolicyWithoutClipboardReads) {
  for (bool allowed : {false, true}) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kDefaultSearchProviderContextMenuAccessAllowed, allowed);
    MenuFuture future;
    delegate().BuildMenuAsync(*main_rfh(), content::ContextMenuParams(),
                              future.GetCallback());

    ASSERT_TRUE(future.IsReady());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(allowed,
              future.Get()->params().properties.contains(
                  prefs::kDefaultSearchProviderContextMenuAccessAllowed));
  }
  EXPECT_TRUE(clipboard()->types_callbacks.empty());
  EXPECT_TRUE(clipboard()->formats_callbacks.empty());
}

TEST_F(ChromeWebContentsViewDelegateViewsClipboardTest,
       ReadsClipboardForPasteCapableMenus) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = ContextMenuDataEditFlags::kCanPaste;

  for (size_t i = 0; i < 3; ++i) {
    MenuFuture future;
    delegate().BuildMenuAsync(*main_rfh(), params, future.GetCallback());
    ASSERT_EQ(i + 1, clipboard()->types_callbacks.size());
    EXPECT_FALSE(future.IsReady());
    std::vector<std::u16string> types;
    if (i != 0) {
      types.push_back(u"text/plain");
    }
    std::move(clipboard()->types_callbacks[i]).Run(std::move(types));
    ASSERT_EQ(i + 1, clipboard()->formats_callbacks.size());
    EXPECT_FALSE(future.IsReady());
    base::flat_set<ui::ClipboardFormatType> formats;
    if (i == 2) {
      formats.insert(ui::ClipboardFormatType::PlainTextType());
    }
    std::move(clipboard()->formats_callbacks[i]).Run(std::move(formats));

    ASSERT_TRUE(future.IsReady());
    ASSERT_TRUE(future.Get());
    const auto& model = future.Get()->menu_model();
    auto paste = model.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE);
    ASSERT_TRUE(paste);
    EXPECT_EQ(i != 0, model.IsEnabledAt(*paste));
    auto paste_and_match_style =
        model.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
    ASSERT_TRUE(paste_and_match_style);
    EXPECT_EQ(i == 2, model.IsEnabledAt(*paste_and_match_style));
  }
}

TEST_F(ChromeWebContentsViewDelegateViewsClipboardTest,
       IgnoresSupersededTypesReply) {
  content::ContextMenuParams params;
  params.edit_flags = ContextMenuDataEditFlags::kCanPaste;
  MenuFuture first_menu;
  delegate().BuildMenuAsync(*main_rfh(), params, first_menu.GetCallback());
  ASSERT_EQ(1u, clipboard()->types_callbacks.size());

  MenuFuture latest_menu;
  delegate().BuildMenuAsync(*main_rfh(), content::ContextMenuParams(),
                            latest_menu.GetCallback());
  ASSERT_TRUE(latest_menu.IsReady());
  ASSERT_TRUE(latest_menu.Get());
  std::move(clipboard()->types_callbacks[0]).Run({u"text/plain"});

  EXPECT_TRUE(clipboard()->formats_callbacks.empty());
  EXPECT_FALSE(first_menu.IsReady());
}

TEST_F(ChromeWebContentsViewDelegateViewsClipboardTest,
       IgnoresSupersededFormatsReply) {
  content::ContextMenuParams params;
  params.edit_flags = ContextMenuDataEditFlags::kCanPaste;
  MenuFuture first_menu;
  delegate().BuildMenuAsync(*main_rfh(), params, first_menu.GetCallback());
  ASSERT_EQ(1u, clipboard()->types_callbacks.size());
  std::move(clipboard()->types_callbacks[0]).Run({u"text/plain"});
  ASSERT_EQ(1u, clipboard()->formats_callbacks.size());

  MenuFuture latest_menu;
  delegate().BuildMenuAsync(*main_rfh(), content::ContextMenuParams(),
                            latest_menu.GetCallback());
  ASSERT_TRUE(latest_menu.IsReady());
  ASSERT_TRUE(latest_menu.Get());
  std::move(clipboard()->formats_callbacks[0])
      .Run({ui::ClipboardFormatType::PlainTextType()});

  EXPECT_FALSE(first_menu.IsReady());
}

}  // namespace

TEST_F(ChromeWebContentsViewFocusHelperTest,
       StoreAndRestoreFocusWhenWidgetActive) {
  ASSERT_TRUE(widget_->IsActive());
  focusable_view_->RequestFocus();
  ASSERT_TRUE(focusable_view_->HasFocus());

  focus_helper()->StoreFocus();
  widget_->GetFocusManager()->ClearFocus();
  ASSERT_FALSE(focusable_view_->HasFocus());
  EXPECT_EQ(focus_helper()->GetStoredFocus(), focusable_view_);

  EXPECT_TRUE(focus_helper()->RestoreFocus());
  EXPECT_TRUE(focusable_view_->HasFocus());
  EXPECT_EQ(focus_helper()->GetStoredFocus(), nullptr);
  EXPECT_FALSE(focus_helper()->RestoreFocus());
}

TEST_F(ChromeWebContentsViewFocusHelperTest, RestoreFocusWhenWidgetInactive) {
  focusable_view_->RequestFocus();
  focus_helper()->StoreFocus();

  std::unique_ptr<views::Widget> other_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  other_widget->Show();
  ASSERT_FALSE(widget_->IsActive());

  ActivationRestoreFocusObserver observer(widget_.get(), focus_helper());
  EXPECT_TRUE(focus_helper()->RestoreFocus());

  EXPECT_TRUE(observer.activation_restore_result());
  EXPECT_TRUE(widget_->IsActive());
  EXPECT_TRUE(focusable_view_->HasFocus());
  EXPECT_EQ(focus_helper()->GetStoredFocus(), nullptr);
}

TEST_F(ChromeWebContentsViewFocusHelperTest,
       DeferredRestoreFocusWhenWidgetRemainsInactive) {
  focusable_view_->RequestFocus();
  focus_helper()->StoreFocus();

  std::unique_ptr<views::Widget> other_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  other_widget->Show();
  ASSERT_FALSE(widget_->IsActive());

  focusable_view_->set_suppress_activation_on_focus(true);
  EXPECT_TRUE(focus_helper()->RestoreFocus());
  ASSERT_FALSE(widget_->IsActive());
  EXPECT_EQ(focus_helper()->GetStoredFocus(), focusable_view_);

  focusable_view_->set_suppress_activation_on_focus(false);
  ActivationRestoreFocusObserver observer(widget_.get(), focus_helper());
  widget_->Activate();

  EXPECT_TRUE(observer.activation_restore_result());
  EXPECT_TRUE(widget_->IsActive());
  EXPECT_TRUE(focusable_view_->HasFocus());
  EXPECT_EQ(focus_helper()->GetStoredFocus(), nullptr);
}
