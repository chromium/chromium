// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/common/open_search_description_document_handler.mojom.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Per-tab search engine manager. Handles dealing search engine processing
// functionality.
class SearchEngineTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchEngineTabHelper>,
      public chrome::mojom::OpenSearchDescriptionDocumentHandler,
      public favicon::FaviconDriverObserver {
 public:
  ~SearchEngineTabHelper() override;

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void WebContentsDestroyed() override;

 private:
  explicit SearchEngineTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SearchEngineTabHelper>;

  // chrome::mojom::OpenSearchDescriptionDocumentHandler overrides.
  void PageHasOpenSearchDescriptionDocument(const GURL& page_url,
                                            const GURL& osdd_url) override;

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(content::NavigationHandle* handle);

  content::WebContentsFrameBindingSet<
      chrome::mojom::OpenSearchDescriptionDocumentHandler>
      osdd_handler_bindings_;

  ScopedObserver<favicon::FaviconDriver, favicon::FaviconDriverObserver>
      favicon_driver_observer_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
