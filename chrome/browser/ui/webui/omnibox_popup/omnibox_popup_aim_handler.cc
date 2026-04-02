// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

namespace {

searchbox::mojom::SearchContextPtr ToSearchContext(
    std::unique_ptr<SearchboxContextData::Context> context) {
  if (!context) {
    return nullptr;
  }

  auto search_context = searchbox::mojom::SearchContext::New();
  search_context->input = context->text;
  search_context->attachments = std::move(context->file_infos);
  search_context->tool_mode = context->mode;
  return search_context;
}

}  // namespace

OmniboxPopupAimHandler::OmniboxPopupAimHandler(
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents) {}

OmniboxPopupAimHandler::~OmniboxPopupAimHandler() = default;

void OmniboxPopupAimHandler::RequestClose() {
  if (auto* aim_popup_content = GetAimPopupContent()) {
    // For screen readers, focus the location bar since the user requested a
    // manual close of the popup, as opposed to the popup closing because the
    // tab navigated somewhere else.
    aim_popup_content->UpdateLocationBarFocusForScreenReader();
  }
  if (embedder_) {
    embedder_->CloseUI();
  }
}

void OmniboxPopupAimHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, nullptr);
  }
}

void OmniboxPopupAimHandler::OnPopupShown(
    std::unique_ptr<SearchboxContextData::Context> context) {
  auto page_context = ToSearchContext(std::move(context));
  CHECK(page_context);
  page_->OnPopupShown(std::move(page_context));
}

void OmniboxPopupAimHandler::SetPreserveContextOnClose(
    bool preserve_context_on_close) {
  page_->SetPreserveContextOnClose(preserve_context_on_close);
}

void OmniboxPopupAimHandler::ClearPopup(
    base::OnceCallback<void(const std::string&)> callback) {
  page_->ClearPopup(std::move(callback));
}

void OmniboxPopupAimHandler::AddContext(
    std::unique_ptr<SearchboxContextData::Context> context) {
  auto search_context = ToSearchContext(std::move(context));
  if (!search_context) {
    return;
  }
  page_->AddContext(std::move(search_context));
}

void OmniboxPopupAimHandler::FocusInput() {
  page_->FocusInput();
}

OmniboxAimPopupWebUIContent* OmniboxPopupAimHandler::GetAimPopupContent() {
  if (!embedder_) {
    return nullptr;
  }
  WebUIContentsWrapper* wrapper =
      static_cast<WebUIContentsWrapper*>(embedder_.get());
  return static_cast<OmniboxAimPopupWebUIContent*>(wrapper->GetHost().get());
}
