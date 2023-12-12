// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_STATIC_PAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_STATIC_PAGE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

namespace lens {

class LensRegionSearchController;

// Class for managing opening the region search experience in a static page in a
// new tab.
class LensStaticPageController : public content::WebContentsObserver {
 public:
  explicit LensStaticPageController(Browser* browser);
  ~LensStaticPageController() override;

  // Starts the region search experience on a static page by first taking a
  // screenshot of the active web contents and then passing that screenshot to
  // a WebUI page in a new tab.
  void OpenStaticPage();

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  void LoadChromeLens(gfx::Image image);
  void StartRegionSearch(content::WebContents* web_contents);
  raw_ptr<Browser> browser_ = nullptr;
  std::unique_ptr<LensRegionSearchController> lens_region_search_controller_;

  base::WeakPtrFactory<LensStaticPageController> weak_ptr_factory_{this};
};

// Class to store a static page controller for launching region search on a
// static page. This allows it to exist across navigations.
class LensStaticPageData : public base::SupportsUserData::Data {
 public:
  LensStaticPageData();
  ~LensStaticPageData() override;
  LensStaticPageData(const LensStaticPageData&) = delete;
  LensStaticPageData& operator=(const LensStaticPageData&) = delete;

  static constexpr char kDataKey[] = "lens_static_page_data";
  std::unique_ptr<LensStaticPageController> lens_static_page_controller;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_STATIC_PAGE_CONTROLLER_H_
