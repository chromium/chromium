// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/clipboard/test/test_clipboard.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

typedef ChromeRenderViewHostTestHarness ChromeWebContentsViewDelegateViewsTest;

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
