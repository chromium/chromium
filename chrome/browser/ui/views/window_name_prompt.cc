// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace chrome {
namespace {

constexpr int kWindowNameFieldId = 1;

class WindowNamePromptDelegate : public ui::DialogModelDelegate {
 public:
  void SetBrowserTitleFromTextField(Browser* browser) {
    browser->SetWindowUserTitle(base::UTF16ToUTF8(
        dialog_model()->GetTextfieldByUniqueId(kWindowNameFieldId)->text()));
  }
};

std::unique_ptr<views::DialogDelegate> CreateWindowNamePrompt(
    Browser* browser) {
  auto bubble_delegate_unique = std::make_unique<WindowNamePromptDelegate>();
  WindowNamePromptDelegate* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(IDS_NAME_WINDOW_PROMPT_TITLE))
          .AddOkButton(base::BindOnce(
              &WindowNamePromptDelegate::SetBrowserTitleFromTextField,
              base::Unretained(bubble_delegate), browser))
          .AddCancelButton(base::DoNothing())
          .AddTextfield(
              l10n_util::GetStringUTF16(IDS_NAME_WINDOW_PROMPT_WINDOW_NAME),
              base::UTF8ToUTF16(browser->user_title()),
              ui::DialogModelTextfield::Params().SetUniqueId(
                  kWindowNameFieldId))
          .SetInitiallyFocusedField(kWindowNameFieldId)
          .Build();

  auto bubble = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::MODAL_TYPE_WINDOW);
  // TODO(pbos): Reconsider whether SetInitiallyFocusedView should imply
  // initially-selected text.
  bubble->SelectAllText(kWindowNameFieldId);
  return bubble;
}

void DoShowWindowNamePrompt(Browser* browser,
                            gfx::NativeView anchor,
                            gfx::NativeWindow context) {
  auto prompt = CreateWindowNamePrompt(browser);
  prompt->SetOwnedByWidget(true);
  views::DialogDelegate::CreateDialogWidget(std::move(prompt), context, anchor)
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
