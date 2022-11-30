// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"

#include "base/check_op.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"

namespace translate {

namespace test_utils {

TranslateBubbleView* GetTranslateBubble(Browser* browser) {
  return TranslateBubbleController::FromWebContents(
             browser->tab_strip_model()->GetActiveWebContents())
      ->GetTranslateBubble();
}

const TranslateBubbleModel* GetCurrentModel(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* view = GetTranslateBubble(browser);

  return view ? view->model() : nullptr;
}

void CloseCurrentBubble(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleController* controller =
      TranslateBubbleController::FromWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  if (controller)
    controller->CloseBubble();
}

void PressTranslate(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* bubble = GetTranslateBubble(browser);
  DCHECK(bubble);

  bubble->TabSelectedAt(1);
}

void PressRevert(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* bubble = GetTranslateBubble(browser);
  DCHECK(bubble);

  bubble->TabSelectedAt(0);
}

void SelectTargetLanguageByDisplayName(Browser* browser,
                                       const std::u16string& display_name) {
  DCHECK(browser);

  TranslateBubbleView* bubble = GetTranslateBubble(browser);
  DCHECK(bubble);

  TranslateBubbleModel* model = bubble->model();
  DCHECK(model);

  // Get index of the language with the matching display name.
  int language_index = -1;
  for (int i = 0; i < model->GetNumberOfTargetLanguages(); ++i) {
    const std::u16string& language_name = model->GetTargetLanguageNameAt(i);

    if (language_name == display_name) {
      language_index = i;
      break;
    }
  }
  DCHECK_GE(language_index, 0);

  // Simulate selecting the correct index of the target language combo box.
  bubble->target_language_combobox_->SetSelectedIndex(language_index);
  bubble->TargetLanguageChanged();
}

}  // namespace test_utils

}  // namespace translate
