// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_ui.h"

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

DiscoverUI::DiscoverUI() {}

DiscoverUI::~DiscoverUI() {}

void DiscoverUI::RegisterMessages(content::WebUI* web_ui) {
  std::vector<std::unique_ptr<DiscoverHandler>> handlers =
      DiscoverManager::Get()->CreateWebUIHandlers();
  for (auto& handler : handlers) {
    handlers_.push_back(handler.get());
    web_ui->AddMessageHandler(std::move(handler));
  }
  initialized_ = true;
}

void DiscoverUI::GetAdditionalParameters(base::DictionaryValue* dict) {
  CHECK(initialized_);
  for (DiscoverHandler* handler : handlers_) {
    handler->GetLocalizedStrings(dict);
  }
}

void DiscoverUI::Show() {
  // TODO.
}

}  // namespace chromeos
