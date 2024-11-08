// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_picker_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

bool IsMediaPickerModalWindow(content::WebContents* web_contents) {
  return web_contents &&
         !web_contents->GetDelegate()->IsNeverComposited(web_contents);
}

views::Widget* CreateMediaPickerDialogWidget(Browser* browser,
                                             content::WebContents* web_contents,
                                             views::WidgetDelegate* delegate,
                                             gfx::NativeWindow context,
                                             gfx::NativeView parent) {
  // If |web_contents| is not a background page then the picker will be shown
  // modal to the web contents. Otherwise, the picker is shown in a separate
  // window.
  views::Widget* widget = nullptr;
  if (IsMediaPickerModalWindow(web_contents)) {
    // Close the extension popup to prevent spoofing.
    if (browser && browser->window() &&
        browser->window()->GetExtensionsContainer()) {
      browser->window()->GetExtensionsContainer()->HideActivePopup();
    }
    widget =
        constrained_window::ShowWebModalDialogViews(delegate, web_contents);
  } else {
#if BUILDFLAG(IS_MAC)
    // On Mac, ModalType::kChild with a null parent isn't allowed - fall back to
    // ModalType::kWindow.
    delegate->SetModalType(ui::mojom::ModalType::kWindow);
#endif
    widget =
        views::DialogDelegate::CreateDialogWidget(delegate, context, parent);
    widget->Show();
  }

  return widget;
}
