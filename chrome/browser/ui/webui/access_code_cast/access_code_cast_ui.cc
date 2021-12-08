// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"

#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/access_code_cast_resources.h"
#include "chrome/grit/access_code_cast_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "ui/base/webui/web_ui_util.h"

///////////////////////////////////////////////////////////////////////////////
//  AccessCodeCast dialog:
///////////////////////////////////////////////////////////////////////////////

AccessCodeCastDialog::AccessCodeCastDialog(
    media_router::MediaCastMode cast_mode)
    : cast_mode_(cast_mode) {
  DVLOG(0) << "AccessCodeCastDialog constructor";
  set_can_resize(false);
}

void AccessCodeCastDialog::Show(media_router::MediaCastMode mode) {
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(),
                        new AccessCodeCastDialog(mode));
}

ui::ModalType AccessCodeCastDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

std::u16string AccessCodeCastDialog::GetDialogTitle() const {
  return std::u16string();
}

GURL AccessCodeCastDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIAccessCodeCastURL);
}

void AccessCodeCastDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void AccessCodeCastDialog::GetDialogSize(gfx::Size* size) const {
  // TODO(b/202529859): Replace these with final values
  const int kDefaultWidth = 600;
  const int kDefaultHeight = 400;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string AccessCodeCastDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("castMode", base::Value(cast_mode_));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void AccessCodeCastDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void AccessCodeCastDialog::OnDialogClosed(const std::string& json_retval) {}

void AccessCodeCastDialog::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool AccessCodeCastDialog::ShouldShowDialogTitle() const {
  return false;
}

// Ensure the WebUI dialog has camera access
void AccessCodeCastDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool AccessCodeCastDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

AccessCodeCastDialog::~AccessCodeCastDialog() = default;

///////////////////////////////////////////////////////////////////////////////
//  AccessCodeCast UI controller:
///////////////////////////////////////////////////////////////////////////////

AccessCodeCastUI::AccessCodeCastUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUIAccessCodeCastHost));
  webui::SetupWebUIDataSource(
      source.get(),
      base::make_span(kAccessCodeCastResources, kAccessCodeCastResourcesSize),
      IDR_ACCESS_CODE_CAST_INDEX_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"back", IDS_ACCESS_CODE_CAST_BACK},
      {"cast", IDS_ACCESS_CODE_CAST_CAST},
      {"close", IDS_CLOSE},
      {"dialogTitle", IDS_ACCESS_CODE_CAST_DIALOG_TITLE},
      {"useCamera", IDS_ACCESS_CODE_CAST_USE_CAMERA},
  };

  source->AddLocalizedStrings(kStrings);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

AccessCodeCastUI::~AccessCodeCastUI() = default;

void AccessCodeCastUI::BindInterface(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void AccessCodeCastUI::CreatePageHandler(
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> receiver) {
  DCHECK(page);

  page_handler_ = std::make_unique<AccessCodeCastHandler>(
      std::move(receiver), std::move(page), Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessCodeCastUI)
