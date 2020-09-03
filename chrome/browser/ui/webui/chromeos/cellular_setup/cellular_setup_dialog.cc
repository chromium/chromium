// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog.h"

#include "base/bind.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/cellular_setup_resources.h"
#include "chrome/grit/cellular_setup_resources_map.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/services/cellular_setup/cellular_setup_impl.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace cellular_setup {

namespace {

// TODO(azeemarshad): Determine the exact height and width of the dialog. The
// current mocks are unclear, so these are just a guess.
constexpr int kDialogHeightPx = 850;
constexpr int kDialogWidthPx = 650;

CellularSetupDialog* dialog_instance = nullptr;

}  // namespace

// static
void CellularSetupDialog::ShowDialog(const std::string& cellular_network_guid) {
  if (dialog_instance) {
    dialog_instance->dialog_window()->Focus();
    return;
  }

  dialog_instance = new CellularSetupDialog();

  // Note: chrome::ShowWebDialog() is used instead of
  // dialog_instance->ShowSystemDialog() because it provides the dialog to
  // ability to switch to full-screen in tablet mode.
  chrome::ShowWebDialog(nullptr /* parent */,
                        ProfileManager::GetActiveUserProfile(),
                        dialog_instance);
}

CellularSetupDialog::CellularSetupDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICellularSetupUrl),
                              base::string16()) {
  set_can_resize(false);
}

CellularSetupDialog::~CellularSetupDialog() = default;

void CellularSetupDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

void CellularSetupDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == dialog_instance);
  dialog_instance = nullptr;

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

CellularSetupDialogUI::CellularSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICellularSetupHost);

  source->DisableTrustedTypesCSP();

  chromeos::cellular_setup::AddLocalizedStrings(source);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_HTML);

  // Note: The |kCellularSetupResourcesSize| and |kCellularSetupResources|
  // fields are defined in the generated file
  // chrome/grit/cellular_setup_resources_map.h.
  for (size_t i = 0; i < kCellularSetupResourcesSize; ++i) {
    source->AddResourcePath(kCellularSetupResources[i].name,
                            kCellularSetupResources[i].value);
  }

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

CellularSetupDialogUI::~CellularSetupDialogUI() = default;

void CellularSetupDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::CellularSetup> receiver) {
  CellularSetupImpl::CreateAndBindToReciever(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(CellularSetupDialogUI)

}  // namespace cellular_setup

}  // namespace chromeos
