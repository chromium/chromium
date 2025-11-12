// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "components/omnibox/browser/searchbox.mojom.h"

OmniboxPopupAimHandler::OmniboxPopupAimHandler(
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
    OmniboxPopupUI* omnibox_popup_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      omnibox_popup_ui_(omnibox_popup_ui) {}

OmniboxPopupAimHandler::~OmniboxPopupAimHandler() = default;

void OmniboxPopupAimHandler::OnShow(
    std::unique_ptr<SearchboxContextData::Context> context) {
  if (!context) {
    page_->OnShow(nullptr);
    return;
  }

  auto search_context = searchbox::mojom::SearchContextStub::New();
  search_context->input = context->text;
  search_context->attachments = std::move(context->file_infos);
  search_context->tool_mode = context->mode;

  page_->OnShow(std::move(search_context));
}

void OmniboxPopupAimHandler::OnClose() {
  page_->OnClose();
}

void OmniboxPopupAimHandler::Close() {
  omnibox_popup_ui_->embedder()->CloseUI();
}
