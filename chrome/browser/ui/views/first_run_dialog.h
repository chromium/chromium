// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
}

class FirstRunDialog : public views::DialogDelegateView {
  METADATA_HEADER(FirstRunDialog, views::DialogDelegateView)

 public:
  class TestApi {
   public:
    explicit TestApi(FirstRunDialog* dialog);
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;
    ~TestApi() = default;

    void SetMakeDefaultCheckboxChecked(bool checked);

   private:
    raw_ptr<FirstRunDialog> dialog_;
  };

  FirstRunDialog(const FirstRunDialog&) = delete;
  FirstRunDialog& operator=(const FirstRunDialog&) = delete;

  using OnCloseCallback =
      base::OnceCallback<void(bool closed_through_accept_button)>;

  // Displays the first run UI for reporting opt-in, import data etc.
  static void Show(base::RepeatingClosure learn_more_callback,
                   OnCloseCallback on_close_callback);

 private:
  FirstRunDialog(base::RepeatingClosure learn_more_callback,
                 OnCloseCallback on_close_callback);
  ~FirstRunDialog() override;

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
  OnCloseCallback on_close_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
