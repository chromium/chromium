// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_H_

#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// `GoogleOneOfferIphTabHelper` shows a notification about Google One offer
// which can come with a Chromebook. The notification is shown when a user
// visits Google Drive or Google Photos. `GoogleOneOfferIphTabHelper` uses
// //components/feature_engagement/ to define trigger conditions, e.g. number of
// times this notification is shown, etc. Those conditions and UI strings will
// be served from the server.
class GoogleOneOfferIphTabHelper : public content::WebContentsObserver {
 public:
  explicit GoogleOneOfferIphTabHelper(content::WebContents* web_contents);
  GoogleOneOfferIphTabHelper(const GoogleOneOfferIphTabHelper&) = delete;
  GoogleOneOfferIphTabHelper& operator=(const GoogleOneOfferIphTabHelper&) =
      delete;

  ~GoogleOneOfferIphTabHelper() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
};

#endif  // CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_H_
