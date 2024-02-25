// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
}

class FirstRunDialog : public views::DialogDelegateView {
  METADATA_HEADER(FirstRunDialog, views::DialogDelegateView)

 public:
  FirstRunDialog(const FirstRunDialog&) = delete;
  FirstRunDialog& operator=(const FirstRunDialog&) = delete;

  // Displays the first run UI for reporting opt-in, import data etc.
  static void Show(base::RepeatingClosure learn_more_callback,
                   base::RepeatingClosure quit_runloop);

 private:
  FirstRunDialog(base::RepeatingClosure learn_more_callback,
                 base::RepeatingClosure quit_runloop);
  ~FirstRunDialog() override;

  // This terminates the nested message-loop.
  void Done();

  // views::DialogDelegate:
  bool Accept() override;

  // views::WidgetDelegate:
  void WindowClosing() override;

  // Used to determine whether the dialog was closed by pressing the accept
  // button. The user might close the dialog by pressing the close button
  // instead, in which we default to disabling metrics reporting.
  bool closed_through_accept_button_ = false;

  raw_ptr<views::Checkbox> make_default_ = nullptr;
  raw_ptr<views::Checkbox> report_crashes_ = nullptr;
  base::RepeatingClosure quit_runloop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
