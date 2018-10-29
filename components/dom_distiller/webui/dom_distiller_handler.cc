// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/webui/dom_distiller_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

GURL GetViewUrlFromArgs(const std::string& scheme,
                        const base::ListValue* args) {
  std::string url;
  if (args->GetString(0, &url)) {
    const GURL gurl(url);
    if (url_utils::IsUrlDistillable(gurl)) {
      return url_utils::GetDistillerViewUrlFromUrl(scheme, gurl);
    }
  }
  return GURL();
}

}  // namespace

DomDistillerHandler::DomDistillerHandler(DomDistillerService* service,
                                         const std::string& scheme)
    : service_(service), article_scheme_(scheme), weak_ptr_factory_(this) {}

DomDistillerHandler::~DomDistillerHandler() {}

void DomDistillerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestEntries",
      base::BindRepeating(&DomDistillerHandler::HandleRequestEntries,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addArticle", base::BindRepeating(&DomDistillerHandler::HandleAddArticle,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectArticle",
      base::BindRepeating(&DomDistillerHandler::HandleSelectArticle,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "viewUrl", base::BindRepeating(&DomDistillerHandler::HandleViewUrl,
                                     base::Unretained(this)));
}

void DomDistillerHandler::HandleAddArticle(const base::ListValue* args) {
  std::string url;
  args->GetString(0, &url);
  GURL gurl(url);
  if (gurl.is_valid()) {
    service_->AddToList(
        gurl, service_->CreateDefaultDistillerPage(
                  web_ui()->GetWebContents()->GetContainerBounds().size()),
        base::Bind(&DomDistillerHandler::OnArticleAdded,
                   base::Unretained(this)));
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("domDistiller.onArticleAddFailed");
  }
}

void DomDistillerHandler::HandleViewUrl(const base::ListValue* args) {
  GURL view_url = GetViewUrlFromArgs(article_scheme_, args);
  if (view_url.is_valid()) {
    web_ui()->GetWebContents()->GetController().LoadURL(
        view_url,
        content::Referrer(),
        ui::PAGE_TRANSITION_GENERATED,
        std::string());
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("domDistiller.onViewUrlFailed");
  }
}

void DomDistillerHandler::HandleSelectArticle(const base::ListValue* args) {
  std::string entry_id;
  args->GetString(0, &entry_id);
  GURL url =
      url_utils::GetDistillerViewUrlFromEntryId(article_scheme_, entry_id);
  DCHECK(url.is_valid());
  web_ui()->GetWebContents()->GetController().LoadURL(
      url,
      content::Referrer(),
      ui::PAGE_TRANSITION_GENERATED,
      std::string());
}

void DomDistillerHandler::HandleRequestEntries(const base::ListValue* args) {
  base::ListValue entries;
  const std::vector<ArticleEntry>& entries_specifics = service_->GetEntries();
  for (auto it = entries_specifics.begin(); it != entries_specifics.end();
       ++it) {
    const ArticleEntry& article = *it;
    DCHECK(IsEntryValid(article));
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("entry_id", article.entry_id());
    std::string title = (!article.has_title() || article.title().empty())
                            ? article.entry_id()
                            : article.title();
    entry->SetString("title", net::EscapeForHTML(title));
    entries.Append(std::move(entry));
  }
  // TODO(nyquist): Write a test that ensures we sanitize the data we send.
  web_ui()->CallJavascriptFunctionUnsafe("domDistiller.onReceivedEntries",
                                         entries);
}

void DomDistillerHandler::OnArticleAdded(bool article_available) {
  // TODO(nyquist): Update this function.
  if (article_available) {
    HandleRequestEntries(nullptr);
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("domDistiller.onArticleAddFailed");
  }
}

}  // namespace dom_distiller
