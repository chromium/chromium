// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/external_protocol_dialog_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id,
    ui::PageTransition page_transition, bool has_user_gesture) {
  std::unique_ptr<ExternalProtocolDialogDelegate> delegate(
      new ExternalProtocolDialogDelegate(url, render_process_host_id,
                                         routing_id));
  if (delegate->program_name().empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }

  // Windowing system takes ownership.
  new ExternalProtocolDialog(std::move(delegate), render_process_host_id,
                             routing_id);
}

ExternalProtocolDialog::~ExternalProtocolDialog() {}

gfx::Size ExternalProtocolDialog::CalculatePreferredSize() const {
  constexpr int kDialogContentWidth = 400;
  return gfx::Size(kDialogContentWidth, GetHeightForWidth(kDialogContentWidth));
}

bool ExternalProtocolDialog::ShouldShowCloseButton() const {
  return false;
}

int ExternalProtocolDialog::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 ExternalProtocolDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return delegate_->GetDialogButtonLabel(button);
}

base::string16 ExternalProtocolDialog::GetWindowTitle() const {
  return delegate_->GetTitleText();
}

bool ExternalProtocolDialog::Cancel() {
  ExternalProtocolHandler::RecordHandleStateMetrics(
      false /* checkbox_selected */, ExternalProtocolHandler::BLOCK);

  // Returning true closes the dialog.
  return true;
}

bool ExternalProtocolDialog::Accept() {
  // We record how long it takes the user to accept an external protocol.  If
  // users start accepting these dialogs too quickly, we should worry about
  // clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                           base::TimeTicks::Now() - creation_time_);

  const bool remember = remember_decision_checkbox_->checked();
  ExternalProtocolHandler::RecordHandleStateMetrics(
      remember, ExternalProtocolHandler::DONT_BLOCK);

  delegate_->DoAccept(delegate_->url(), remember);

  // Returning true closes the dialog.
  return true;
}

ui::ModalType ExternalProtocolDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

ExternalProtocolDialog::ExternalProtocolDialog(
    std::unique_ptr<const ProtocolDialogDelegate> delegate,
    int render_process_host_id,
    int routing_id)
    : delegate_(std::move(delegate)),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      creation_time_(base::TimeTicks::Now()) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  DCHECK(delegate_->GetMessageText().empty());
  remember_decision_checkbox_ =
      new views::Checkbox(delegate_->GetCheckboxText());
  AddChildView(remember_decision_checkbox_);

  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id_, routing_id_);
  // Only launch the dialog if there is a web contents associated with the
  // request.
  if (web_contents)
    constrained_window::ShowWebModalDialogViews(this, web_contents);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTERNAL_PROTOCOL);
}
