// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_OMNIBOX_CLIENT_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/webui/new_tab_page/composebox/base_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"
#include "components/metrics/metrics_provider.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class Profile;
class TemplateURLRef;

namespace content {
class WebContents;
}

namespace composebox::mojom {
class PageHandler;
}  // namespace composebox::mojom

namespace lens::proto {
class LensOverlaySuggestInputs;
}  // namespace lens::proto

namespace composebox {

class ComposeboxOmniboxClient final : public ContextualOmniboxClient {
 public:
  ComposeboxOmniboxClient(Profile* profile,
                          content::WebContents* web_contents,
                          BaseComposeboxHandler* composebox_handler);

  ~ComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  raw_ptr<BaseComposeboxHandler> composebox_handler_;
};

}  // namespace composebox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_OMNIBOX_CLIENT_H_
