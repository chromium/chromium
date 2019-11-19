// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

MockTabModalConfirmDialogDelegate::MockTabModalConfirmDialogDelegate(
    content::WebContents* web_contents,
    Delegate* delegate)
    : TabModalConfirmDialogDelegate(web_contents), delegate_(delegate) {}

MockTabModalConfirmDialogDelegate::~MockTabModalConfirmDialogDelegate() {}

base::string16 MockTabModalConfirmDialogDelegate::GetTitle() {
  return base::string16();
}

base::string16 MockTabModalConfirmDialogDelegate::GetDialogMessage() {
  return base::string16();
}

void MockTabModalConfirmDialogDelegate::OnAccepted() {
  if (delegate_)
    delegate_->OnAccepted();
}

void MockTabModalConfirmDialogDelegate::OnCanceled() {
  if (delegate_)
    delegate_->OnCanceled();
}

void MockTabModalConfirmDialogDelegate::OnClosed() {
  if (delegate_)
    delegate_->OnClosed();
}

TabModalConfirmDialogTest::TabModalConfirmDialogTest()
    : delegate_(NULL),
      dialog_(NULL),
      accepted_count_(0),
      canceled_count_(0),
      closed_count_(0) {}

void TabModalConfirmDialogTest::SetUpOnMainThread() {
  auto delegate = std::make_unique<MockTabModalConfirmDialogDelegate>(
      browser()->tab_strip_model()->GetActiveWebContents(), this);
  delegate_ = delegate.get();
  dialog_ = TabModalConfirmDialog::Create(
      std::move(delegate),
      browser()->tab_strip_model()->GetActiveWebContents());
  content::RunAllPendingInMessageLoop();
}

void TabModalConfirmDialogTest::TearDownOnMainThread() {
  content::RunAllPendingInMessageLoop();
}

void TabModalConfirmDialogTest::OnAccepted() {
  ++accepted_count_;
}

void TabModalConfirmDialogTest::OnCanceled() {
  ++canceled_count_;
}

void TabModalConfirmDialogTest::OnClosed() {
  ++closed_count_;
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Accept) {
  dialog_->AcceptTabModalDialog();
  EXPECT_EQ(1, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
  EXPECT_EQ(0, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Cancel) {
  dialog_->CancelTabModalDialog();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(1, canceled_count_);
  EXPECT_EQ(0, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, CancelSelf) {
  delegate_->Cancel();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(1, canceled_count_);
  EXPECT_EQ(0, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Close) {
  dialog_->CloseDialog();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
  EXPECT_EQ(1, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, CloseSelf) {
  delegate_->Close();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
  EXPECT_EQ(1, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Navigate) {
  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser()->tab_strip_model()->GetActiveWebContents()->OpenURL(params);

  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
  EXPECT_EQ(1, closed_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Quit) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  RunUntilBrowserProcessQuits();

  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
  EXPECT_EQ(1, closed_count_);
}
