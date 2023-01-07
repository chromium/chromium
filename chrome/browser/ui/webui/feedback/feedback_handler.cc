// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feedback/child_web_dialog.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

void ShowChildPage(Profile* profile,
                   const FeedbackDialog* dialog,
                   const GURL& url,
                   const std::u16string& title,
                   int dialog_width = 640,
                   int dialog_height = 400,
                   bool can_resize = true,
                   bool can_minimize = true) {
  bool isParentModal = dialog->GetWidget()->IsModal();
  // when the dialog is closed, it will delete itself
  ChildWebDialog* child_dialog = new ChildWebDialog(
      profile, dialog->GetWidget(), url, title,
      /*modal_type=*/
      isParentModal ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_NONE, dialog_width,
      dialog_height, can_resize, can_minimize);

  child_dialog->Show();
}

GURL ChildPageURL(const std::string& child_page) {
  return GURL(base::StrCat({chrome::kChromeUIFeedbackURL, child_page}));
}
}  // namespace

FeedbackHandler::FeedbackHandler(const FeedbackDialog* dialog)
    : dialog_(dialog) {}

FeedbackHandler::~FeedbackHandler() = default;

void FeedbackHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showDialog", base::BindRepeating(&FeedbackHandler::HandleShowDialog,
                                        base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "showAssistantLogsInfo",
      base::BindRepeating(&FeedbackHandler::HandleShowAssistantLogsInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showBluetoothLogsInfo",
      base::BindRepeating(&FeedbackHandler::HandleShowBluetoothLogsInfo,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "showSystemInfo",
      base::BindRepeating(&FeedbackHandler::HandleShowSystemInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showMetrics", base::BindRepeating(&FeedbackHandler::HandleShowMetrics,
                                         base::Unretained(this)));
}

void FeedbackHandler::HandleShowDialog(const base::Value::List& args) {
  dialog_->Show();
}

#if BUILDFLAG(IS_CHROMEOS)
void FeedbackHandler::HandleShowAssistantLogsInfo(
    const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                ChildPageURL("html/assistant_logs_info.html"), std::u16string(),
                /*dialog_width=*/400, /*dialog_height=*/120,
                /*can_resize=*/false, /*can_minimize=*/false);
}
void FeedbackHandler::HandleShowBluetoothLogsInfo(
    const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                ChildPageURL("html/bluetooth_logs_info.html"), std::u16string(),
                /*dialog_width=*/400, /*dialog_height=*/120,
                /*can_resize=*/false, /*can_minimize=*/false);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FeedbackHandler::HandleShowSystemInfo(const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                ChildPageURL("html/sys_info.html"),
                l10n_util::GetStringUTF16(IDS_FEEDBACK_SYSINFO_PAGE_TITLE));
}

void FeedbackHandler::HandleShowMetrics(const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                GURL("chrome://histograms"), std::u16string());
}
