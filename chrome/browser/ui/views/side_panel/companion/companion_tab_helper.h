// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_

#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class ImageModel;
}  // namespace ui

namespace companion {

class CompanionPageHandler;

// A per-tab class that facilitates the showing of the Companion side panel with
// values such as a text query. This class also owns the
// CompanionSidePanelController.
class CompanionTabHelper
    : public content::WebContentsUserData<CompanionTabHelper>,
      public content::WebContentsObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Creates the companion SidePanelEntry and registers it to the contextual
    // registry.
    virtual void CreateAndRegisterEntry() = 0;
    // Deregisters the companion SidePanelEntry.
    virtual void DeregisterEntry() = 0;
    // Shows the companion side panel.
    virtual void ShowCompanionSidePanel(
        SidePanelOpenTrigger side_panel_open_trigger) = 0;
    // Triggers an update of the 'open in new tab' button.
    virtual void UpdateNewTabButton(GURL url_to_open) = 0;
    // Called when the companion side panel is closed.
    virtual void OnCompanionSidePanelClosed() = 0;
    // Retrieves the web contents for testing purposes.
    virtual content::WebContents* GetCompanionWebContentsForTesting() = 0;
    // Returns true if Companion is currently open and showing to the user.
    virtual bool IsCompanionShowing() = 0;
    // Set Companion as the active side panel entry in the given web contents.
    virtual void SetCompanionAsActiveEntry(content::WebContents* contents) = 0;
    // Add a callback to be called when Companion is fully loaded in the side
    // panel, i.e. the spinner of the tab would stop spinning, Javascript is
    // loaded and the onload event was dispatched.
    virtual void AddCompanionFinishedLoadingCallback(
        base::OnceCallback<void()> callback) = 0;
    // Create a contextual Lens view to open in the companion panel.
    virtual void OpenContextualLensView(
        const content::OpenURLParams& params) = 0;
    // Testing method to get the Lens view web contents.
    virtual content::WebContents* GetLensViewWebContentsForTesting() = 0;
    // Testing method to open lens results in a new tab.
    virtual bool OpenLensResultsInNewTabForTesting() = 0;
    // Testing method to check if the new tab button is enabled for Lens.
    virtual bool IsLensLaunchButtonEnabledForTesting() = 0;
  };

  using CompanionLoadedCallback = base::OnceCallback<void()>;

  CompanionTabHelper(const CompanionTabHelper&) = delete;
  CompanionTabHelper& operator=(const CompanionTabHelper&) = delete;
  ~CompanionTabHelper() override;

  // content::WebContentsObserver overrides.
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // Add a callback to be called when Companion is fully loaded in the side
  // panel, i.e. the spinner of the tab would stop spinning, Javascript is
  // loaded and the onload event was dispatched.
  void AddCompanionFinishedLoadingCallback(CompanionLoadedCallback callback);

  // Shows the companion side panel.
  void ShowCompanionSidePanel(SidePanelOpenTrigger trigger);
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
      const std::string& content_type);

  // Returns the latest text query set by the client or an empty string if none.
  // Clears the last previous query after returning a copy.
  std::string GetTextQuery();
  // Starts the region search controller with the specified parameters.
  void StartRegionSearch(content::WebContents* web_contents,
                         bool use_fullscreen_capture,
                         bool force_open_in_new_tab = false,
                         lens::AmbientSearchEntryPoint entry_point =
                             lens::AmbientSearchEntryPoint::
                                 CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS);

  // Returns the latest image data saved to the helper and not passed to the
  // handler or an empty pointer if none.
  std::unique_ptr<side_panel::mojom::ImageQuery> GetImageQuery();
  bool HasImageQuery();

  // Returns the most recently set start time for the text query entry point.
  std::unique_ptr<base::Time> GetTextQueryStartTime();

  // Called when the companion side panel is closed. Used for cleaning up any
  // local state.
  void OnCompanionSidePanelClosed();

  // Triggers the companion side panel entry to be created and registered for
  // the tab.
  void CreateAndRegisterEntry();
  // Triggers the companion side panel entry to be deregistered for the tab.
  void DeregisterEntry();

  // Triggers an update for the 'open in new tab' button in the side panel
  // header to make sure the visibility is correct.
  void UpdateNewTabButton(GURL url_to_open);

  base::WeakPtr<CompanionPageHandler> GetCompanionPageHandler();
  void SetCompanionPageHandler(
      base::WeakPtr<CompanionPageHandler> companion_page_handler);

  // Returns the companion web contents for testing purposes.
  content::WebContents* GetCompanionWebContentsForTesting();

  // For caching entry point metrics.
  // Called to cache the trigger which is later recorded as metrics as soon as
  // the companion page opens up.
  void SetMostRecentSidePanelOpenTrigger(
      std::optional<SidePanelOpenTrigger> side_panel_open_trigger);
  // Called to get the most recent value of trigger and immediately reset it.
  std::optional<SidePanelOpenTrigger>
  GetAndResetMostRecentSidePanelOpenTrigger();

  // Create and register a contextual Lens entry.
  void CreateAndRegisterLensEntry(const content::OpenURLParams& params,
                                  std::u16string combobox_label,
                                  const ui::ImageModel favicon);
  // Open an already created contextual Lens view with provided params.
  void OpenContextualLensView(const content::OpenURLParams& params);
  // Delete the contextual lens view associated with this web contents.
  void RemoveContextualLensView();
  content::WebContents* GetLensViewWebContentsForTesting();
  bool OpenLensResultsInNewTabForTesting();
  bool IsLensLaunchButtonEnabledForTesting();

 private:
  explicit CompanionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<CompanionTabHelper>;

  // Sets the latest text query and shows the side panel with that query.
  void SetTextQuery(const std::string& text_query);

  // Sets appropriate source and target language parameters and translate
  // filter.
  GURL SetImageTranslateQueryParams(GURL upload_url);

  // Extracts the text query from a query parameter contained in the search URL.
  // Returns an empty string if the value does not exist.
  std::string GetTextQueryFromSearchUrl(const GURL& search_url) const;

  std::unique_ptr<side_panel::mojom::ImageQuery> image_query_;
  std::unique_ptr<Delegate> delegate_;
  std::string text_query_;
  std::unique_ptr<base::Time> text_query_start_time_;

  // Caches the trigger source for an in-progress companion page open action in
  // the current tab. Should be cleared after the open action is complete.
  std::optional<SidePanelOpenTrigger> side_panel_open_trigger_;
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

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_TAB_HELPER_H_
