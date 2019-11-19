// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"

class MockTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  class Delegate {
   public:
    virtual void OnAccepted() = 0;
    virtual void OnCanceled() = 0;
    virtual void OnClosed() = 0;

   protected:
    virtual ~Delegate() {}
  };

  MockTabModalConfirmDialogDelegate(content::WebContents* web_contents,
                                    Delegate* delegate);
  ~MockTabModalConfirmDialogDelegate() override;

  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;

  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockTabModalConfirmDialogDelegate);
};

class TabModalConfirmDialogTest
    : public InProcessBrowserTest,
      public MockTabModalConfirmDialogDelegate::Delegate {
 public:
  TabModalConfirmDialogTest();

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // MockTabModalConfirmDialogDelegate::Delegate:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

 protected:
  // Owned by |dialog_|.
  MockTabModalConfirmDialogDelegate* delegate_;

  // Deletes itself.
  TabModalConfirmDialog* dialog_;

  int accepted_count_;
  int canceled_count_;
  int closed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogTest);
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
