// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/read_later_resources.h"
#include "chrome/grit/read_later_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace {
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/read_later/";

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  base::string16 str = l10n_util::GetStringUTF16(id);
  base::Erase(str, '&');
  source->AddString(message, str);
}
}  // namespace

ReadLaterUI::ReadLaterUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIReadLaterHost);
  source->AddResourcePath("read_later.mojom-lite.js",
                          IDR_READ_LATER_MOJO_LITE_JS);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"readHeader", IDS_READ_LATER_MENU_READ_HEADER},
      {"title", IDS_READ_LATER_TITLE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMarkAsRead", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_READ},
      {"tooltipMarkAsUnread", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_UNREAD},
      {"unreadHeader", IDS_READ_LATER_MENU_UNREAD_HEADER},
  };
  for (const auto& str : kLocalizedStrings)
    AddLocalizedString(source, str.name, str.id);

  webui::SetupWebUIDataSource(
      source, base::make_span(kReadLaterResources, kReadLaterResourcesSize),
      kGeneratedPath, IDR_READ_LATER_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

ReadLaterUI::~ReadLaterUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadLaterUI)

void ReadLaterUI::BindInterface(
    mojo::PendingReceiver<read_later::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReadLaterUI::CreatePageHandler(
    mojo::PendingRemote<read_later::mojom::Page> page,
    mojo::PendingReceiver<read_later::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ReadLaterPageHandler>(std::move(receiver),
                                                         std::move(page));
}
