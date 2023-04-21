// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_

#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

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
    // Triggers an update of the 'open in new tab' button
    virtual void UpdateNewTabButtonState() = 0;
  };

  CompanionTabHelper(const CompanionTabHelper&) = delete;
  CompanionTabHelper& operator=(const CompanionTabHelper&) = delete;
  ~CompanionTabHelper() override;

  // Shows the companion side panel with query provided by the |search_url|.
  void ShowCompanionSidePanelForSearchURL(const GURL& search_url);
  // Shows the companion side panel with the image bytes passed via
  // |thumbnail_data|.
  void ShowCompanionSidePanelForImage(
      const GURL& src_url,
      const bool is_image_translate,
      const std::string& additional_query_params_modified,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& original_size,
      const gfx::Size& downscaled_size,
      const std::string& image_extension,
      const std::string& content_type);

  // Returns the latest text query set by the client or an empty string if none.
  // Clears the last previous query after returning a copy.
  std::string GetTextQuery();
  // Sets the latest text query and shows the side panel with that query.
  void SetTextQuery(const std::string& text_query);
  // Starts the region search controller with the specified parameters.
  void StartRegionSearch(content::WebContents* web_contents,
                         bool use_fullscreen_capture);

  // Returns the latest image data saved to the helper and not passed to the
  // handler or an empty pointer if none.
  std::unique_ptr<side_panel::mojom::ImageQuery> GetImageQuery();

  // Triggers an update for the 'open in new tab' button in the side panel
  // header to make sure the visibility is correct.
  void UpdateNewTabButtonState();
  // Returns the latest set url to be used for the 'open in new tab' button in
  // the side panel header.
  GURL GetNewTabButtonUrl();

  base::WeakPtr<CompanionPageHandler> GetCompanionPageHandler();
  void SetCompanionPageHandler(
      base::WeakPtr<CompanionPageHandler> companion_page_handler);

 private:
  explicit CompanionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CompanionTabHelper>;

  // Sets appropriate source and target language parameters and translate
  // filter.
  GURL SetImageTranslateQueryParams(GURL upload_url);

  // Extracts the text query from a query parameter contained in the search URL.
  // Returns an empty string if the value does not exist.
  std::string GetTextQueryFromSearchUrl(const GURL& search_url) const;

  std::unique_ptr<side_panel::mojom::ImageQuery> image_query_;
  std::unique_ptr<Delegate> delegate_;
  std::string text_query_;
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  // A weak reference to the last-created WebUI object for this web contents.
  base::WeakPtr<CompanionPageHandler> companion_page_handler_;

  base::WeakPtrFactory<CompanionTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
