// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_static_page_controller.h"

#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/lens_metrics.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace lens {

LensStaticPageData::LensStaticPageData() = default;
LensStaticPageData::~LensStaticPageData() = default;

LensStaticPageController::LensStaticPageController(Browser* browser)
    : browser_(browser) {}
LensStaticPageController::~LensStaticPageController() = default;

void LensStaticPageController::OpenStaticPage() {
  DCHECK(browser_);
  // Take a screenshot of the active web contents and save it to profile user
  // data.
  content::WebContents* active_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  gfx::Rect fullscreen_size = gfx::Rect(active_web_contents->GetSize());
  // TODO(crbug/1383279): Refactor screenshot code shared here with code in
  // image_editor::ScreenshotFlow.
#if BUILDFLAG(IS_MAC)
  const gfx::NativeView& native_view =
      active_web_contents->GetContentNativeView();
  gfx::Image img;
  bool rval = ui::GrabViewSnapshot(native_view, fullscreen_size, &img);
  // If |img| is empty, clients should treat it as a canceled action, but
  // we have a DCHECK for development as we expected this call to succeed.
  DCHECK(rval);
  LoadChromeLens(img);
#else
  ui::GrabWindowSnapshotAsyncCallback load_url_callback =
      base::BindOnce(&LensStaticPageController::LoadChromeLens,
                     weak_ptr_factory_.GetWeakPtr());
  const gfx::NativeWindow& native_window = active_web_contents->GetNativeView();
  ui::GrabWindowSnapshotAsync(native_window, fullscreen_size,
                              std::move(load_url_callback));
#endif
}

void LensStaticPageController::LoadChromeLens(gfx::Image image) {
  DCHECK(browser_);
  auto region_search_data = std::make_unique<RegionSearchCapturedData>();
  region_search_data->image = image;
  browser_->profile()->SetUserData(RegionSearchCapturedData::kDataKey,
                                   std::move(region_search_data));

  // After we have set the image data, we can open the WebUI tab and finish
  // displaying the region search experience.
  GURL url(chrome::kChromeUILensURL);
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  params.initiator_origin = url::Origin::Create(url);
  content::WebContents* new_tab = browser_->OpenURL(params);
  // Observe the new web contents in order to start the region search controller
  // once it is properly loaded (this prevents the region search controller from
  // closing prematurely).
  Observe(new_tab);
}

void LensStaticPageController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  StartRegionSearch(web_contents());
}

void LensStaticPageController::StartRegionSearch(
    content::WebContents* contents) {
  DCHECK(contents);
  DCHECK(browser_);
  if (!lens_region_search_controller_) {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>();
  }
  lens_region_search_controller_->Start(
      contents,
      /*use_fullscreen_capture=*/false,
      /*is_google_default_search_provider=*/true,
      lens::AmbientSearchEntryPoint::
          CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS);
}

}  // namespace lens
