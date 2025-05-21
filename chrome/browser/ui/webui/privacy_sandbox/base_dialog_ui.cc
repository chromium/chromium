// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"

#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/privacy_sandbox/dialog_view_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/privacy_sandbox_resources.h"
#include "chrome/grit/privacy_sandbox_resources_map.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace privacy_sandbox {

using dialog::mojom::BaseDialogPage;
using dialog::mojom::BaseDialogPageHandler;

BaseDialogUI::BaseDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIPrivacySandboxBaseDialogHost);
  webui::SetupWebUIDataSource(source, kPrivacySandboxResources,
                              IDR_PRIVACY_SANDBOX_BASE_DIALOG_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"adPrivacyPageTitle", IDS_SETTINGS_AD_PRIVACY_PAGE_TITLE}};

  source->AddLocalizedStrings(kStrings);
  privacy_sandbox::DialogViewContext* view_context =
      privacy_sandbox::DialogViewContext::FromWebContents(
          web_ui->GetWebContents());
  if (view_context) {
    delegate_ = &view_context->GetDelegate();
    source->AddInteger(
        "noticeIdToShow",
        static_cast<int32_t>(delegate_->GetPrivacySandboxNotice()));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(BaseDialogUI)

BaseDialogUI::~BaseDialogUI() = default;

void BaseDialogUI::BindInterface(
    mojo::PendingReceiver<BaseDialogPageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void BaseDialogUI::CreatePageHandler(
    mojo::PendingRemote<BaseDialogPage> page,
    mojo::PendingReceiver<BaseDialogPageHandler> receiver) {
  // Checks that the PendingRemote is bound.
  CHECK(page);
  if (auto* privacy_sandbox_notice_service =
          PrivacySandboxNoticeServiceFactory::GetForProfile(
              Profile::FromWebUI(web_ui()))) {
    page_handler_ = std::make_unique<BaseDialogHandler>(
        std::move(receiver), std::move(page),
        privacy_sandbox_notice_service->GetDesktopViewManager(), delegate_);
  }
}

}  // namespace privacy_sandbox
