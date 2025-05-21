// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

using InfoBarType = infobar_internals::mojom::InfoBarType;
using InfoBarEntry = infobar_internals::mojom::InfoBarEntry;

InfoBarInternalsHandler::InfoBarInternalsHandler(
    mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

InfoBarInternalsHandler::~InfoBarInternalsHandler() = default;

void InfoBarInternalsHandler::TriggerInfoBar(InfoBarType type,
                                             TriggerInfoBarCallback callback) {
  std::move(callback).Run(TriggerInfoBarInternal(type));
}

void InfoBarInternalsHandler::GetInfoBars(GetInfoBarsCallback callback) {
  static const base::NoDestructor<std::array<InfoBarEntry, 1>> kInfobars(
      {InfoBarEntry{InfoBarType::kInstallerDownloader,
                    "Installer Downloader"}});

  std::vector<infobar_internals::mojom::InfoBarEntryPtr> infobar_list;
  for (const auto& infobar : *kInfobars) {
    infobar_list.emplace_back(InfoBarEntry::New(infobar.type, infobar.name));
  }

  std::move(callback).Run(std::move(infobar_list));
}

bool InfoBarInternalsHandler::TriggerInfoBarInternal(InfoBarType type) {
  switch (type) {
    case InfoBarType::kInstallerDownloader: {
      // TODO(htpts://crbug.com/417727976): Trigger manually installer
      // downloader infobar.
      //
      // Not implemented yet.
      return false;
    }
  }

  return false;
}
