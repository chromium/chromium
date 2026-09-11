// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_picker_utils.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

class MediaPickerUtilsTest : public InProcessBrowserTest {
 public:
  MediaPickerUtilsTest() = default;
  ~MediaPickerUtilsTest() override = default;
};

IN_PROC_BROWSER_TEST_F(MediaPickerUtilsTest, CreateMediaPickerDialogWidget) {
  // Setup for opening a media picker.
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  views::DialogDelegate delegate;
  delegate.SetModalType(ui::mojom::ModalType::kChild);
  gfx::NativeWindow context = web_contents->GetTopLevelNativeWindow();
#if defined(USE_AURA)
  gfx::NativeView web_contents_parent = web_contents->GetTopLevelNativeWindow();
#else
  gfx::NativeView web_contents_parent = web_contents->GetContentNativeView();
#endif

  // Open the picker with the web contents as the parent.
  views::Widget* widget = CreateMediaPickerDialogWidget(
      browser(), web_contents, &delegate, context, web_contents_parent);

  // The picker is created and its parent is the tab web contents.
  ASSERT_TRUE(widget);
#if defined(USE_AURA)
  EXPECT_EQ(widget->GetNativeWindow()->parent(), web_contents_parent);
#endif

  widget->CloseNow();
}

IN_PROC_BROWSER_TEST_F(MediaPickerUtilsTest,
                       CreateMediaPickerDialogWidget_ExtensionPopup) {
  // Pretend the active tab is an extension popup.
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
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

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(MediaPickerUtilsTest,
                       CreateMediaPickerDialogWidget_FloatingCompanionContext) {
  // Verify that an unparented picker created with a floating context widget
  // (e.g. Omnibox Everywhere) inherits kFloatingWindow on macOS.
  views::Widget::InitParams floating_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  floating_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  auto floating_context_widget = std::make_unique<views::Widget>();
  floating_context_widget->Init(std::move(floating_params));
  floating_context_widget->Show();

  views::DialogDelegate floating_delegate;
  floating_delegate.SetModalType(ui::mojom::ModalType::kChild);
  views::Widget* floating_dialog = CreateMediaPickerDialogWidget(
      /*browser=*/nullptr, /*web_contents=*/nullptr, &floating_delegate,
      floating_context_widget->GetNativeWindow(),
      /*parent=*/gfx::NativeView());

  ASSERT_TRUE(floating_dialog);
  EXPECT_EQ(floating_dialog->GetZOrderLevel(),
            ui::ZOrderLevel::kFloatingWindow);
  floating_dialog->CloseNow();

  // Verify that a non-floating context widget does NOT inherit kFloatingWindow.
  views::Widget::InitParams normal_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  normal_params.z_order = ui::ZOrderLevel::kNormal;
  auto normal_context_widget = std::make_unique<views::Widget>();
  normal_context_widget->Init(std::move(normal_params));
  normal_context_widget->Show();

  views::DialogDelegate normal_delegate;
  normal_delegate.SetModalType(ui::mojom::ModalType::kChild);
  views::Widget* normal_dialog = CreateMediaPickerDialogWidget(
      /*browser=*/nullptr, /*web_contents=*/nullptr, &normal_delegate,
      normal_context_widget->GetNativeWindow(),
      /*parent=*/gfx::NativeView());

  ASSERT_TRUE(normal_dialog);
  EXPECT_EQ(normal_dialog->GetZOrderLevel(), ui::ZOrderLevel::kNormal);
  normal_dialog->CloseNow();
}
#endif  // BUILDFLAG(IS_MAC)
