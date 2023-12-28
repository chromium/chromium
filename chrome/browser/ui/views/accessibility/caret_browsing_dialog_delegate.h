// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CARET_BROWSING_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CARET_BROWSING_DIALOG_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class PrefService;

namespace views {
class Checkbox;
}

// A dialog box that confirms that the user wants to enable caret browsing.
class CaretBrowsingDialogDelegate : public views::DialogDelegateView {
  METADATA_HEADER(CaretBrowsingDialogDelegate, views::DialogDelegateView)

 public:
  static void Show(gfx::NativeWindow parent_window, PrefService* pref_service);

 private:
  explicit CaretBrowsingDialogDelegate(PrefService* pref_service);
  CaretBrowsingDialogDelegate(const CaretBrowsingDialogDelegate&) = delete;
  CaretBrowsingDialogDelegate& operator=(const CaretBrowsingDialogDelegate&) =
      delete;
  ~CaretBrowsingDialogDelegate() override;

  const raw_ptr<PrefService> pref_service_;

  // Checkbox where the user can say they don't want to be asked when they
  // toggle caret browsing next time.
  raw_ptr<views::Checkbox> do_not_ask_checkbox_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CARET_BROWSING_DIALOG_DELEGATE_H_
