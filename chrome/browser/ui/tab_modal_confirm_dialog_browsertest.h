// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_

#include "base/memory/raw_ptr.h"
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

  MockTabModalConfirmDialogDelegate(const MockTabModalConfirmDialogDelegate&) =
      delete;
  MockTabModalConfirmDialogDelegate& operator=(
      const MockTabModalConfirmDialogDelegate&) = delete;

  ~MockTabModalConfirmDialogDelegate() override;

  std::u16string GetTitle() override;
  std::u16string GetDialogMessage() override;

  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

 private:
  raw_ptr<Delegate> delegate_;
};

class TabModalConfirmDialogTest
    : public InProcessBrowserTest,
      public MockTabModalConfirmDialogDelegate::Delegate {
 public:
  TabModalConfirmDialogTest();

  TabModalConfirmDialogTest(const TabModalConfirmDialogTest&) = delete;
  TabModalConfirmDialogTest& operator=(const TabModalConfirmDialogTest&) =
      delete;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // MockTabModalConfirmDialogDelegate::Delegate:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

 protected:
  // Owned by |dialog_|.
  raw_ptr<MockTabModalConfirmDialogDelegate, AcrossTasksDanglingUntriaged>
      delegate_;

  // Deletes itself.
  raw_ptr<TabModalConfirmDialog, AcrossTasksDanglingUntriaged> dialog_;

  int accepted_count_;
  int canceled_count_;
  int closed_count_;
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
