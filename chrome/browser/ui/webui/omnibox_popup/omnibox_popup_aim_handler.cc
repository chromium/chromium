// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace {

searchbox::mojom::SearchContextStubPtr ToSearchContext(
    std::unique_ptr<SearchboxContextData::Context> context) {
  if (!context) {
    return nullptr;
  }

  auto search_context = searchbox::mojom::SearchContextStub::New();
  search_context->input = context->text;
  search_context->attachments = std::move(context->file_infos);
  search_context->tool_mode = context->mode;
  return search_context;
}

}  // namespace

OmniboxPopupAimHandler::OmniboxPopupAimHandler(
    mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
    OmniboxPopupUI* omnibox_popup_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      omnibox_popup_ui_(omnibox_popup_ui) {}

OmniboxPopupAimHandler::~OmniboxPopupAimHandler() = default;

void OmniboxPopupAimHandler::RequestClose() {
  omnibox_popup_ui_->embedder()->CloseUI();
}

void OmniboxPopupAimHandler::NavigateCurrentTab(const GURL& url) {
  auto* browser_window_interface = webui::GetBrowserWindowInterface(
      omnibox_popup_ui_->web_ui()->GetWebContents());
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser_window_interface->OpenURL(params, base::NullCallback());
}

void OmniboxPopupAimHandler::OnWidgetShown(
    std::unique_ptr<SearchboxContextData::Context> context) {
  auto page_context = ToSearchContext(std::move(context));
  CHECK(page_context);
  page_->OnWidgetShown(std::move(page_context));
}

void OmniboxPopupAimHandler::SetPreserveContextOnClose(
    bool preserve_context_on_close) {
  page_->SetPreserveContextOnClose(preserve_context_on_close);
}

void OmniboxPopupAimHandler::OnWidgetClosed() {
  // Unretained() is safe because `page_` is a mojo remote owned by `this`.
  page_->OnWidgetClosed(base::BindOnce(
      &OmniboxPopupAimHandler::OnClosedCallback, base::Unretained(this)));
}

void OmniboxPopupAimHandler::AddContext(
    std::unique_ptr<SearchboxContextData::Context> context) {
  auto search_context = ToSearchContext(std::move(context));
  if (!search_context) {
    return;
  }
  page_->AddContext(std::move(search_context));
}

void OmniboxPopupAimHandler::OnClosedCallback(const std::string& input) {
  WebUIContentsWrapper* wrapper =
      static_cast<WebUIContentsWrapper*>(omnibox_popup_ui_->embedder().get());
  OmniboxAimPopupWebUIContent* aim_popup_content =
      static_cast<OmniboxAimPopupWebUIContent*>(wrapper->GetHost().get());
  if (aim_popup_content) {
    aim_popup_content->OnPageClosedWithInput(input);
  }
}
