// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_

#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace companion {

class CompanionPageHandler;

// A per-tab class that facilitates the showing of the Companion side panel with
// values such as a text query. This class also owns the
// CompanionSidePanelController.
class CompanionTabHelper
    : public content::WebContentsUserData<CompanionTabHelper> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Shows the companion side panel.
    virtual void ShowCompanionSidePanel() = 0;
  };

  CompanionTabHelper(const CompanionTabHelper&) = delete;
  CompanionTabHelper& operator=(const CompanionTabHelper&) = delete;
  ~CompanionTabHelper() override;

  void ShowCompanionSidePanel(const GURL& search_url);

  // Returns the latest text query set by the client or an empty string if none.
  // Clears the last previous query after returning a copy.
  std::string GetTextQuery();
  // Sets the latest text query and shows the side panel with that query.
  void SetTextQuery(const std::string& text_query);

  base::WeakPtr<CompanionPageHandler> GetCompanionPageHandler();
  void SetCompanionPageHandler(
      base::WeakPtr<CompanionPageHandler> companion_page_handler);

 private:
  explicit CompanionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CompanionTabHelper>;

  // Extracts the text query from a query parameter contained in the search URL.
  // Returns an empty string if the value does not exist.
  std::string GetTextQueryFromSearchUrl(const GURL& search_url) const;

  std::unique_ptr<Delegate> delegate_;
  std::string text_query_;
  // A weak reference to the last-created WebUI object for this web contents.
  base::WeakPtr<CompanionPageHandler> companion_page_handler_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
