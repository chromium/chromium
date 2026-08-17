// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace {

class TestRenderViewContextMenuViews : public RenderViewContextMenuViews {
 public:
  TestRenderViewContextMenuViews(content::RenderFrameHost& rfh,
                                 const content::ContextMenuParams& params)
      : RenderViewContextMenuViews(rfh,
                                   params,
                                   /*is_paste_enabled=*/false,
                                   /*is_paste_and_match_style_enabled=*/false) {
  }

  TestRenderViewContextMenuViews(const TestRenderViewContextMenuViews&) =
      delete;
  TestRenderViewContextMenuViews& operator=(
      const TestRenderViewContextMenuViews&) = delete;

  ~TestRenderViewContextMenuViews() override = default;

  using RenderViewContextMenuViews::GetAcceleratorForCommandId;
  using RenderViewContextMenuViews::IsCommandIdChecked;
  using RenderViewContextMenuViews::IsCommandIdEnabled;

  // Overridden as a no-op to prevent the production views code from calling
  // `views::MenuRunner::RunMenuAt()`, which would spin a nested message loop
  // and block the test thread synchronously (causing a test timeout). This
  // matches the pattern in `TestRenderViewContextMenu::Show()`.
  void Show() override {}
};

class RenderViewContextMenuViewsBrowserTest : public InProcessBrowserTest {
 public:
  content::RenderFrameHost& GetPrimaryMainFrame() const {
    return *browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame();
  }
};

IN_PROC_BROWSER_TEST_F(RenderViewContextMenuViewsBrowserTest,
                       WritingDirectionVisibleForEditablePlugin) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
  params.writing_direction_default =
      blink::ContextMenuData::kCheckableMenuItemEnabled |
      blink::ContextMenuData::kCheckableMenuItemChecked;
  params.writing_direction_left_to_right =
      blink::ContextMenuData::kCheckableMenuItemEnabled;
  params.writing_direction_right_to_left =
      blink::ContextMenuData::kCheckableMenuItemEnabled;
  params.page_url = GURL("http://www.example.com/");

  TestRenderViewContextMenuViews menu(GetPrimaryMainFrame(), params);
  menu.Init();

  EXPECT_TRUE(menu.menu_model()
                  .GetIndexOfCommandId(kWritingDirectionMenuId)
                  .has_value());
  EXPECT_TRUE(menu.IsCommandIdEnabled(kWritingDirectionDefaultId));
  EXPECT_TRUE(menu.IsCommandIdChecked(kWritingDirectionDefaultId));
  EXPECT_TRUE(menu.IsCommandIdEnabled(IDC_WRITING_DIRECTION_LTR));
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_WRITING_DIRECTION_LTR));
  EXPECT_TRUE(menu.IsCommandIdEnabled(IDC_WRITING_DIRECTION_RTL));
  EXPECT_FALSE(menu.IsCommandIdChecked(IDC_WRITING_DIRECTION_RTL));
}

IN_PROC_BROWSER_TEST_F(RenderViewContextMenuViewsBrowserTest,
                       WritingDirectionHiddenForNonEditablePlugin) {
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
  params.writing_direction_default =
      blink::ContextMenuData::kCheckableMenuItemDisabled;
  params.writing_direction_left_to_right =
      blink::ContextMenuData::kCheckableMenuItemDisabled;
  params.writing_direction_right_to_left =
      blink::ContextMenuData::kCheckableMenuItemDisabled;
  params.page_url = GURL("http://www.example.com/");

  TestRenderViewContextMenuViews menu(GetPrimaryMainFrame(), params);
  menu.Init();

  EXPECT_FALSE(menu.menu_model()
                   .GetIndexOfCommandId(kWritingDirectionMenuId)
                   .has_value());
  EXPECT_FALSE(menu.IsCommandIdEnabled(kWritingDirectionDefaultId));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_WRITING_DIRECTION_LTR));
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_WRITING_DIRECTION_RTL));
}

IN_PROC_BROWSER_TEST_F(RenderViewContextMenuViewsBrowserTest,
                       DictationAcceleratorSetFromPref) {
  constexpr char kTestVoiceTypingHotkey[] = "Ctrl+Shift+D";
  browser()->GetProfile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey,
                                                 kTestVoiceTypingHotkey);
  content::ContextMenuParams params;
  TestRenderViewContextMenuViews menu(GetPrimaryMainFrame(), params);
  ui::Accelerator accel;
  EXPECT_TRUE(
      menu.GetAcceleratorForCommandId(IDC_CONTENT_CONTEXT_DICTATION, &accel));
  EXPECT_EQ(ui::VKEY_D, accel.key_code());
  EXPECT_TRUE(accel.IsCtrlDown());
  EXPECT_TRUE(accel.IsShiftDown());
}

}  // namespace
