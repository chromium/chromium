// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_RICH_RADIO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_RICH_RADIO_BUTTON_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace extensions {

// A compound button encapsulating a RadioButton and supporting icon, text and
// descriptive subtext. This is intended to look similar to the
// RichRadioButton widget currently implemented on Android. It is conceptually
// reusable for any application, but it's starting off as a piece of an
// Extensions Views-based dialog.
//
// TODO(http://crbug.com/461806299):
// - Ensure that when initially shown, no radio button is selected or focused.

class RichRadioButton : public views::Button {
 public:
  METADATA_HEADER(RichRadioButton, views::Button)

 public:
  RichRadioButton(const ui::ImageModel& image,
                  const std::u16string& title,
                  const std::u16string& description,
                  int group_id,
                  base::RepeatingClosure on_selected_callback);
  ~RichRadioButton() override;

  // Returns the checked state of the underlying RadioButton.
  bool GetCheckedForTesting() const;

 private:
  raw_ptr<views::RadioButton> radio_button_ = nullptr;
  base::CallbackListSubscription subscription_;
};

BEGIN_VIEW_BUILDER(, RichRadioButton, views::Button)
END_VIEW_BUILDER

}  // namespace extensions

DEFINE_VIEW_BUILDER(, extensions::RichRadioButton)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_RICH_RADIO_BUTTON_H_
