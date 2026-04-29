// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_picker_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/aura/window.h"
#include "ui/views/window/dialog_delegate.h"

class MediaPickerUtilsTest : public InProcessBrowserTest {
 public:
  MediaPickerUtilsTest() = default;
  ~MediaPickerUtilsTest() override = default;
};

IN_PROC_BROWSER_TEST_F(MediaPickerUtilsTest, CreateMediaPickerDialogWidget) {
  // Setup for opening a media picker.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::DialogDelegate delegate;
  delegate.SetModalType(ui::mojom::ModalType::kChild);
  gfx::NativeWindow context = web_contents->GetTopLevelNativeWindow();
  gfx::NativeWindow web_contents_parent =
      web_contents->GetTopLevelNativeWindow();

  // Open the picker with the web contents as the parent.
  views::Widget* widget = CreateMediaPickerDialogWidget(
      browser(), web_contents, &delegate, context, web_contents_parent);

  // The picker is created and its parent is the tab web contents.
  ASSERT_TRUE(widget);
  EXPECT_EQ(widget->GetNativeWindow()->parent(), web_contents_parent);

  widget->CloseNow();
}

IN_PROC_BROWSER_TEST_F(MediaPickerUtilsTest,
                       CreateMediaPickerDialogWidget_ExtensionPopup) {
  // Pretend the active tab is an extension popup.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::SetViewType(web_contents,
                          extensions::mojom::ViewType::kExtensionPopup);

  // Setup for opening a media picker.
  views::DialogDelegate delegate;
  delegate.SetModalType(ui::mojom::ModalType::kChild);
  gfx::NativeWindow context = web_contents->GetTopLevelNativeWindow();

  // Open the picker as it's done from DesktopMediaPickerDialogView.
  views::Widget* widget = CreateMediaPickerDialogWidget(
      /*browser=*/nullptr, web_contents, &delegate, context,
      /*parent=*/gfx::NativeView());

  // The picker is created and is not modal to a tab or extension popup.
  ASSERT_TRUE(widget);
  ui::mojom::ModalType modal_type = widget->widget_delegate()->GetModalType();
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(modal_type, ui::mojom::ModalType::kSystem);
#else
  EXPECT_EQ(modal_type, ui::mojom::ModalType::kNone);
#endif
  widget->CloseNow();
}
