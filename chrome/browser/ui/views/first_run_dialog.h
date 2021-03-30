// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_

#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class Checkbox;
}

class FirstRunDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(FirstRunDialog);

  FirstRunDialog(const FirstRunDialog&) = delete;
  FirstRunDialog& operator=(const FirstRunDialog&) = delete;

  // Displays the first run UI for reporting opt-in, import data etc.
  static void Show(Profile* profile);

 private:
  explicit FirstRunDialog(Profile* profile);
  ~FirstRunDialog() override;

  // This terminates the nested message-loop.
  void Done();

  // views::DialogDelegate:
  bool Accept() override;

  // views::WidgetDelegate:
  void WindowClosing() override;

  views::Checkbox* make_default_ = nullptr;
  views::Checkbox* report_crashes_ = nullptr;
  base::RepeatingClosure quit_runloop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
