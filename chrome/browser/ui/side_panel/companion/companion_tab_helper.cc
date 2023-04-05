// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"

#include "chrome/browser/ui/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace companion {

CompanionTabHelper::CompanionTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<CompanionTabHelper>(*web_contents),
      delegate_(CreateDelegate(web_contents)) {}

CompanionTabHelper::~CompanionTabHelper() = default;

void CompanionTabHelper::ShowCompanionSidePanel(const GURL& search_url) {
  CHECK(delegate_);
  SetTextQuery(GetTextQueryFromSearchUrl(search_url));
  delegate_->ShowCompanionSidePanel();
}

void CompanionTabHelper::SetCompanionPageHandler(
    base::WeakPtr<CompanionPageHandler> companion_page_handler) {
  CHECK(companion_page_handler);
  companion_page_handler_ = companion_page_handler;
}

base::WeakPtr<CompanionPageHandler>
CompanionTabHelper::GetCompanionPageHandler() {
  return companion_page_handler_;
}

std::string CompanionTabHelper::GetTextQuery() {
  std::string copy = text_query_;
  text_query_.clear();
  return copy;
}

void CompanionTabHelper::SetTextQuery(const std::string& text_query) {
  CHECK(!text_query.empty());
  text_query_ = text_query;
  if (companion_page_handler_) {
    companion_page_handler_->OnSearchTextQuery(GetTextQuery());
  }
}

std::string CompanionTabHelper::GetTextQueryFromSearchUrl(
    const GURL& search_url) const {
  std::string text_query_param_value;
  if (!net::GetValueForKeyInQuery(search_url, "q", &text_query_param_value)) {
    return std::string();
  }
  return text_query_param_value;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CompanionTabHelper);

}  // namespace companion
