// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace chrome {
namespace {

void ConfigureLayout(views::View* contents_view) {
  auto* layout =
      contents_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetInteriorMargin(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
}

void SetBrowserTitleFromTextField(Browser* browser, views::Textfield* field) {
  browser->SetWindowUserTitle(base::UTF16ToUTF8(field->GetText()));
}

std::unique_ptr<views::DialogDelegateView> CreateWindowNamePrompt(
    Browser* browser) {
  auto delegate = std::make_unique<views::DialogDelegateView>();

  ConfigureLayout(delegate.get());

  auto* field = delegate->AddChildView(std::make_unique<views::Textfield>());
  field->SetText(base::UTF8ToUTF16(browser->user_title()));
  // TODO(ellyjones): the field should probably be initially focused?
  // Needs WidgetDelegate::SetInitiallyFocusedView().

  delegate->SetOwnedByWidget(true);
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->SetTitle(l10n_util::GetStringUTF16(IDS_NAME_WINDOW_PROMPT_TITLE));
  delegate->SetAcceptCallback(
      base::BindOnce(&SetBrowserTitleFromTextField, browser, field));

  return delegate;
}

void DoShowWindowNamePrompt(Browser* browser,
                            gfx::NativeView anchor,
                            gfx::NativeWindow context) {
  views::DialogDelegate::CreateDialogWidget(CreateWindowNamePrompt(browser),
                                            context, anchor)
      ->Show();
}

}  // namespace

void ShowWindowNamePrompt(Browser* browser) {
  const gfx::NativeView anchor = BrowserView::GetBrowserViewForBrowser(browser)
                                     ->GetWidget()
                                     ->GetNativeView();
  // It's fine to pass in a null context, since it will be inferred from the
  // non-null anchor.
  DoShowWindowNamePrompt(browser, anchor, gfx::kNullNativeWindow);
}

void ShowWindowNamePromptForTesting(Browser* browser,
                                    gfx::NativeWindow context) {
  DoShowWindowNamePrompt(browser, gfx::kNullNativeView, context);
}

}  // namespace chrome
