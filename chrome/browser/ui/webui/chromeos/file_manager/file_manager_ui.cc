// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/file_manager/file_manager_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/file_manager/file_manager_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace file_manager {

FileManagerUI::FileManagerUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFileManagerHost);
  source->AddResourcePath("file_manager.css", IDR_FILE_MANAGER_CSS);
  source->AddResourcePath("file_manager.js", IDR_FILE_MANAGER_JS);
  source->AddResourcePath("file_manager.mojom-lite.js",
                          IDR_FILE_MANAGER_MOJO_LITE_JS);
  source->AddResourcePath("browser_proxy.js", IDR_FILE_MANAGER_PROXY_JS);

  // Default content for chrome://file-manager: ensures unhandled URLs return
  // 404 rather than content from SetDefaultResource().
  source->AddResourcePath("", IDR_FILE_MANAGER_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

FileManagerUI::~FileManagerUI() = default;

void FileManagerUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void FileManagerUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> pending_page,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<FileManagerPageHandler>(
      std::move(pending_page_handler), std::move(pending_page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FileManagerUI)

}  // namespace file_manager
}  // namespace chromeos
