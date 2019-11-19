// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"

#include "base/logging.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"

namespace translate {

namespace test_utils {

const TranslateBubbleModel* GetCurrentModel(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* view = TranslateBubbleView::GetCurrentBubble();
  return view ? view->model() : nullptr;
}

void PressTranslate(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* bubble = TranslateBubbleView::GetCurrentBubble();
  DCHECK(bubble);

  views::LabelButton button(nullptr, base::string16());
  button.SetID(TranslateBubbleView::BUTTON_ID_TRANSLATE);

  bubble->ButtonPressed(&button,
                        ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                     ui::DomCode::ENTER, ui::EF_NONE));
}

void PressRevert(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* bubble = TranslateBubbleView::GetCurrentBubble();
  DCHECK(bubble);

  views::LabelButton button(nullptr, base::string16());
  button.SetID(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);

  bubble->ButtonPressed(&button,
                        ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                     ui::DomCode::ENTER, ui::EF_NONE));
}

void SelectTargetLanguageByDisplayName(Browser* browser,
                                       const base::string16& display_name) {
  DCHECK(browser);

  TranslateBubbleView* bubble = TranslateBubbleView::GetCurrentBubble();
  DCHECK(bubble);

  TranslateBubbleModel* model = bubble->model();
  DCHECK(model);

  // Get index of the language with the matching display name.
  int language_index = -1;
  for (int i = 0; i < model->GetNumberOfLanguages(); ++i) {
    const base::string16& language_name = model->GetLanguageNameAt(i);

    if (language_name == display_name) {
      language_index = i;
      break;
    }
  }
  DCHECK_GE(language_index, 0);

  // Simulate selecting the correct index of the target language combo box.
  bubble->target_language_combobox_->SetSelectedIndex(language_index);
  bubble->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_TARGET_LANGUAGE);
}

}  // namespace test_utils

}  // namespace translate
