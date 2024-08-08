// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace {

void ShowChildPage(Profile* profile,
                   const base::WeakPtr<FeedbackDialog>& dialog,
                   const GURL& url,
                   const std::u16string& title,
                   const std::string& args = "",
                   int dialog_width = 640,
                   int dialog_height = 400,
                   bool can_resize = true,
                   bool can_minimize = true) {
  CHECK(profile);
  if (!dialog) {
    return;
  }

  const bool is_parent_modal = dialog->GetWidget()->IsModal();

  auto delegate = std::make_unique<ui::WebDialogDelegate>();
  delegate->set_dialog_args(args);
  delegate->set_dialog_content_url(url);
  delegate->set_dialog_modal_type(is_parent_modal
                                      ? ui::mojom::ModalType::kSystem
                                      : ui::mojom::ModalType::kNone);
  delegate->set_dialog_size(gfx::Size(dialog_width, dialog_height));
  delegate->set_dialog_title(title);
  delegate->set_minimum_dialog_size(gfx::Size(400, 120));
  delegate->set_can_maximize(true);
  delegate->set_can_minimize(can_minimize);
  delegate->set_can_resize(can_resize);
  delegate->set_show_dialog_title(true);

  chrome::ShowWebDialog(
      dialog->GetWidget()->GetNativeView(),
      // The delegate is self-deleting once the dialog is shown.
      profile, delegate.release());
}

GURL ChildPageURL(const std::string& child_page) {
  return GURL(base::StrCat({chrome::kChromeUIFeedbackURL, child_page}));
}
}  // namespace

FeedbackHandler::FeedbackHandler(base::WeakPtr<FeedbackDialog> dialog)
    : dialog_(std::move(dialog)) {}

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
#endif  // BUILDFLAG(IS_CHROMEOS)

  web_ui()->RegisterMessageCallback(
      "showAutofillMetadataInfo",
      base::BindRepeating(&FeedbackHandler::HandleShowAutofillMetadataInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showSystemInfo",
      base::BindRepeating(&FeedbackHandler::HandleShowSystemInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showMetrics", base::BindRepeating(&FeedbackHandler::HandleShowMetrics,
                                         base::Unretained(this)));
}

void FeedbackHandler::HandleShowDialog(const base::Value::List& args) {
  if (dialog_) {
    dialog_->Show();
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void FeedbackHandler::HandleShowAssistantLogsInfo(
    const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                ChildPageURL("html/assistant_logs_info.html"), std::u16string(),
                std::string(),
                /*dialog_width=*/400, /*dialog_height=*/120,
                /*can_resize=*/false, /*can_minimize=*/false);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FeedbackHandler::HandleShowAutofillMetadataInfo(
    const base::Value::List& args) {
  DCHECK(!args.empty());
  ShowChildPage(
      Profile::FromWebUI(web_ui()), dialog_,
      ChildPageURL("html/autofill_metadata_info.html"),
      l10n_util::GetStringUTF16(IDS_FEEDBACK_AUTOFILL_METADATA_PAGE_TITLE),
      args.front().GetString());
}

void FeedbackHandler::HandleShowSystemInfo(const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                ChildPageURL("html/system_info.html"),
                l10n_util::GetStringUTF16(IDS_FEEDBACK_SYSINFO_PAGE_TITLE));
}

void FeedbackHandler::HandleShowMetrics(const base::Value::List& args) {
  ShowChildPage(Profile::FromWebUI(web_ui()), dialog_,
                GURL("chrome://histograms"), std::u16string());
}
