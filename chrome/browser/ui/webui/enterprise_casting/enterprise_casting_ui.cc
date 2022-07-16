// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/enterprise_casting/enterprise_casting_ui.h"

#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/enterprise_casting_resources.h"
#include "chrome/grit/enterprise_casting_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"

///////////////////////////////////////////////////////////////////////////////
//  EnterpriseCasting dialog:
///////////////////////////////////////////////////////////////////////////////

EnterpriseCastingDialog::EnterpriseCastingDialog(
    media_router::MediaCastMode cast_mode)
    : cast_mode_(cast_mode) {
  DVLOG(0) << "EnterpriseCastingDialog constructor";
  set_can_resize(false);
}

void EnterpriseCastingDialog::Show(media_router::MediaCastMode mode) {
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(),
                        new EnterpriseCastingDialog(mode));
}

ui::ModalType EnterpriseCastingDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

std::u16string EnterpriseCastingDialog::GetDialogTitle() const {
  return std::u16string();
}

GURL EnterpriseCastingDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIEnterpriseCastingURL);
}

void EnterpriseCastingDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void EnterpriseCastingDialog::GetDialogSize(gfx::Size* size) const {
  // TODO(b/202529859): Replace these with final values
  const int kDefaultWidth = 600;
  const int kDefaultHeight = 400;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string EnterpriseCastingDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("castMode", base::Value(cast_mode_));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void EnterpriseCastingDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void EnterpriseCastingDialog::OnDialogClosed(const std::string& json_retval) {}

void EnterpriseCastingDialog::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool EnterpriseCastingDialog::ShouldShowDialogTitle() const {
  return false;
}

// Ensure the WebUI dialog has camera access
void EnterpriseCastingDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool EnterpriseCastingDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

EnterpriseCastingDialog::~EnterpriseCastingDialog() = default;

///////////////////////////////////////////////////////////////////////////////
//  EnterpriseCasting UI controller:
///////////////////////////////////////////////////////////////////////////////

EnterpriseCastingUI::EnterpriseCastingUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUIEnterpriseCastingHost));
  webui::SetupWebUIDataSource(source.get(),
                              base::make_span(kEnterpriseCastingResources,
                                              kEnterpriseCastingResourcesSize),
                              IDR_ENTERPRISE_CASTING_INDEX_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

EnterpriseCastingUI::~EnterpriseCastingUI() = default;

void EnterpriseCastingUI::BindInterface(
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void EnterpriseCastingUI::CreatePageHandler(
    mojo::PendingRemote<enterprise_casting::mojom::Page> page,
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandler> receiver) {
  DCHECK(page);

  page_handler_ = std::make_unique<EnterpriseCastingHandler>(
      std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseCastingUI)
