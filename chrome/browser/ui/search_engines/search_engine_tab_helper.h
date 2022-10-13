// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/common/open_search_description_document_handler.mojom.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/find_in_page/find_notification_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class NavigationEntry;
class RenderFrameHost;
}  // namespace content

// Per-tab search engine manager. Handles dealing search engine processing
// functionality.
class SearchEngineTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchEngineTabHelper>,
      public chrome::mojom::OpenSearchDescriptionDocumentHandler,
      public favicon::FaviconDriverObserver {
 public:
  // Binds to the supplied receiver if `rfh` is the outermost frame in a
  // WebContents. Each WebContents could have multiple outermost frames, e.g.
  // the primary main frame, prerendering main frames, and main frames stored in
  // the back-forward cache.
  static void BindOpenSearchDescriptionDocumentHandler(
      content::RenderFrameHost* rfh,
      mojo::PendingReceiver<chrome::mojom::OpenSearchDescriptionDocumentHandler>
          receiver);

  SearchEngineTabHelper(const SearchEngineTabHelper&) = delete;
  SearchEngineTabHelper& operator=(const SearchEngineTabHelper&) = delete;
  SearchEngineTabHelper(SearchEngineTabHelper&&) = delete;
  SearchEngineTabHelper& operator=(SearchEngineTabHelper&&) = delete;

  ~SearchEngineTabHelper() override;

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void WebContentsDestroyed() override;

 protected:
  explicit SearchEngineTabHelper(content::WebContents* web_contents);
  // Virtual for testing.
  virtual std::u16string GenerateKeywordFromNavigationEntry(
      content::NavigationEntry* entry);

 private:
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

  mojo::ReceiverSet<chrome::mojom::OpenSearchDescriptionDocumentHandler>
      osdd_handler_receivers_;

  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      favicon_driver_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
