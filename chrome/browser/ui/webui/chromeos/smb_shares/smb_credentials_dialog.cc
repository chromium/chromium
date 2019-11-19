// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_credentials_dialog.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace smb_dialog {
namespace {

constexpr int kSmbCredentialsDialogHeight = 250;

void AddSmbCredentialsDialogStrings(content::WebUIDataSource* html_source) {
  static const struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"smbCredentialsDialogTitle", IDS_SMB_SHARES_CREDENTIALS_DIALOG_TITLE},
      {"smbCredentialsUsername", IDS_SMB_SHARES_CREDENTIALS_USERNAME},
      {"smbCredentialsPassword", IDS_SMB_SHARES_CREDENTIALS_PASSWORD},
      {"save", IDS_SAVE},
      {"cancel", IDS_CANCEL},
  };
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
}

std::string GetDialogId(int32_t mount_id) {
  return chrome::kChromeUISmbCredentialsURL + base::NumberToString(mount_id);
}

}  // namespace

// static
void SmbCredentialsDialog::Show(int32_t mount_id,
                                const std::string& share_path) {
  // If an SmbCredentialsDialog is already opened for |mount_id|, focus that
  // dialog rather than opening a second one.
  auto* instance = SystemWebDialogDelegate::FindInstance(GetDialogId(mount_id));
  if (instance) {
    instance->Focus();
    return;
  }

  SmbCredentialsDialog* dialog = new SmbCredentialsDialog(mount_id, share_path);
  dialog->ShowSystemDialog();
}

SmbCredentialsDialog::SmbCredentialsDialog(int32_t mount_id,
                                           const std::string& share_path)
    : SystemWebDialogDelegate(GURL(GetDialogId(mount_id)),
                              base::string16() /* title */),
      mount_id_(mount_id),
      share_path_(share_path) {}

SmbCredentialsDialog::~SmbCredentialsDialog() = default;

void SmbCredentialsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth,
                kSmbCredentialsDialogHeight);
}

std::string SmbCredentialsDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("mid", base::Value(mount_id_));
  args.SetKey("path", base::Value(share_path_));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

SmbCredentialsDialogUI::SmbCredentialsDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISmbCredentialsHost);

  AddSmbCredentialsDialogStrings(source);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_SMB_CREDENTIALS_DIALOG_CONTAINER_HTML);
  source->AddResourcePath("smb_credentials_dialog.html",
                          IDR_SMB_CREDENTIALS_DIALOG_HTML);
  source->AddResourcePath("smb_credentials_dialog.js",
                          IDR_SMB_CREDENTIALS_DIALOG_JS);

  web_ui->AddMessageHandler(
      std::make_unique<SmbHandler>(Profile::FromWebUI(web_ui)));

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

SmbCredentialsDialogUI::~SmbCredentialsDialogUI() = default;

}  // namespace smb_dialog
}  // namespace chromeos
