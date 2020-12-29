// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/file_manager/file_manager_ui.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chromeos/components/file_manager/file_manager_page_handler.h"
#include "chromeos/components/file_manager/url_constants.h"
#include "chromeos/grit/chromeos_file_manager_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/file_manager/grit/file_manager_gen_resources_map.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/file_manager/grit/file_manager_resources_map.h"

namespace chromeos {
namespace file_manager {

void AddFilesAppResources(content::WebUIDataSource* source,
                          const GritResourceMap* entries,
                          size_t size) {
  for (size_t i = 0; i < size; ++i) {
    std::string path(entries[i].name);
    // Only load resources for Files app.
    if (base::StartsWith(path, "file_manager/")) {
      // Files app UI has all paths relative to //ui/file_manager/file_manager/
      // so we remove the leading file_manager/ to match the existing paths.
      base::ReplaceFirstSubstringAfterOffset(&path, 0, "file_manager/", "");
      source->AddResourcePath(path, entries[i].value);
    }
  }
}

FileManagerUI::FileManagerUI(content::WebUI* web_ui,
                             std::unique_ptr<FileManagerUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  auto* trusted_source = CreateTrustedAppDataSource();
  content::WebUIDataSource::Add(browser_context, trusted_source);
}

content::WebUIDataSource* FileManagerUI::CreateTrustedAppDataSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chromeos::file_manager::kChromeUIFileManagerHost);

  // Setup chrome://file-manager main and default page.
  source->AddResourcePath("", IDR_FILE_MANAGER_SWA_MAIN_HTML);
  source->SetDefaultResource(IDR_FILE_MANAGER_SWA_MAIN_HTML);

  // Add chrome://file-manager content.
  source->AddResourcePath("main.js", IDR_FILE_MANAGER_SWA_MAIN_JS);
  source->AddResourcePath("file_manager_private_fakes.js",
                          IDR_FILE_MANAGER_SWA_FILE_MANAGER_PRIVATE_FAKES_JS);
  source->AddResourcePath("file_manager_fakes.js",
                          IDR_FILE_MANAGER_SWA_FILE_MANAGER_FAKES_JS);
  source->AddResourcePath("file_manager.mojom-lite.js",
                          IDR_FILE_MANAGER_SWA_MOJO_LITE_JS);
  source->AddResourcePath("browser_proxy.js",
                          IDR_FILE_MANAGER_SWA_BROWSER_PROXY_JS);
  source->AddResourcePath("script_loader.js",
                          IDR_FILE_MANAGER_SWA_SCRIPT_LOADER_JS);

  AddFilesAppResources(source, kFileManagerResources,
                       kFileManagerResourcesSize);
  AddFilesAppResources(source, kFileManagerGenResources,
                       kFileManagerGenResourcesSize);

  // Load time data: add files app strings and feature flags.
  delegate_->PopulateLoadTimeData(source);
  source->UseStringsJs();

  // Script security policy.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
      "chrome://resources "
      "'self' ;");

  // Metadata Shared Worker security policy.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
      "'self' ;");

  // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();

  return source;
}

FileManagerUI::~FileManagerUI() = default;

void FileManagerUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (page_factory_receiver_.is_bound())
    page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void FileManagerUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> pending_page,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<FileManagerPageHandler>(
      this, std::move(pending_page_handler), std::move(pending_page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FileManagerUI)

}  // namespace file_manager
}  // namespace chromeos
