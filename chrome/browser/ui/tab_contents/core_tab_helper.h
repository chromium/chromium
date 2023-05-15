// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_rendering_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace content {
struct OpenURLParams;
}

// Per-tab class to handle functionality that is core to the operation of tabs.
class CoreTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<CoreTabHelper> {
 public:
  CoreTabHelper(const CoreTabHelper&) = delete;
  CoreTabHelper& operator=(const CoreTabHelper&) = delete;

  ~CoreTabHelper() override;

  // Initial title assigned to NavigationEntries from Navigate.
  static std::u16string GetDefaultTitle();

  // Returns a human-readable description the tab's loading state.
  std::u16string GetStatusText() const;

  void UpdateContentRestrictions(int content_restrictions);

  // Encodes the given image to proper image format and adds to |search_args|
  // thumbnail image content data. Returns the format the image was encoded to.
  // Public for testing.
  static lens::mojom::ImageFormat EncodeImageIntoSearchArgs(
      const gfx::Image& image,
      TemplateURLRef::SearchTermsArgs& search_args);

  // Open the Lens standalone experience for the image that triggered the
  // context menu. If the google lens supports opening requests in side panel,
  // then the request will open in the side panel instead of new tab.
  void SearchWithLens(content::RenderFrameHost* render_frame_host,
                      const GURL& src_url,
                      lens::EntryPoint entry_point,
                      bool is_image_translate);

  // Open the Lens experience for an image. Used for sending the bitmap selected
  // via Lens Region Search. |image_original_size| is specified in case of
  // resizing that happens prior to passing the image to |CoreTabHelper|. If
  // the search engine supports opening requests in side panel, then the request
  // will open in the side panel instead of a new tab.
  void RegionSearchWithLens(gfx::Image image,
                            const gfx::Size& image_original_size,
                            std::vector<lens::mojom::LatencyLogPtr> log_data);

  // Perform an image search for the image that triggered the context menu.  The
  // |src_url| is passed to the search request and is not used directly to fetch
  // the image resources. If the search engine supports opening requests in side
  // panel, then the request will open in the side panel instead of a new tab.
  void SearchByImage(content::RenderFrameHost* render_frame_host,
                     const GURL& src_url);

  // Same as above, with ability to specify that the image should be translated.
  void SearchByImage(content::RenderFrameHost* render_frame_host,
                     const GURL& src_url,
                     bool is_image_translate);

  // Performs an image search for the provided image. If the search engine
  // supports opening requests in side panel, then the request will open in side
  // panel instead of a new tab.
  void SearchByImage(const gfx::Image& image,
                     const gfx::Size& image_original_size);

  void set_new_tab_start_time(const base::TimeTicks& time) {
    new_tab_start_time_ = time;
  }

  base::TimeTicks new_tab_start_time() const { return new_tab_start_time_; }
  int content_restrictions() const { return content_restrictions_; }

  std::unique_ptr<content::WebContents> SwapWebContents(
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load);

 private:
  explicit CoreTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<CoreTabHelper>;

  static bool GetStatusTextForWebContents(std::u16string* status_text,
                                          content::WebContents* source);

  // content::WebContentsObserver overrides:
  void DidStartLoading() override;
  void NavigationEntriesDeleted() override;
  void OnWebContentsFocused(content::RenderWidgetHost*) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost*) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void DoSearchByImage(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      const GURL& src_url,
      const std::string& additional_query_params,
      bool use_side_panel,
      bool is_image_translate,
      const std::string& thumbnail_content_type,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& original_size,
      const gfx::Size& downscaled_size,
      const std::string& image_extension,
      const std::vector<lens::mojom::LatencyLogPtr> latency_logs);

  // Wrapper method for fetching template URL service.
  TemplateURLService* GetTemplateURLService();

  // Encode the image and set the content type and image format for the
  // result image. Returns the vector of image bytes.
  static std::vector<unsigned char> EncodeImage(
      const gfx::Image& image,
      std::string& content_type,
      lens::mojom::ImageFormat& image_format);

  // Helper function to return if the companion side panel is enabled for image
  // search.
  bool IsImageSearchSupportedForCompanion();

  // Posts the bytes and content type to the specified URL If |use_side_panel|
  // is true, the content will open in a side panel, otherwise it will open in
  // a new tab. If we are waiting for a Lens ping response, this will instead
  // set stored_lens_search_settings_.
  void PostContentToURL(TemplateURLRef::PostContent post_content,
                        GURL url,
                        bool use_side_panel);

  // Opens |open_url_params|. If |use_side_panel| is true, the content will open
  // in a side panel, otherwise it will open in a new tab.
  void OpenOpenURLParams(content::OpenURLParams open_url_params,
                         bool use_side_panel);

  // Create a thumbnail to POST to search engine for the image that triggered
  // the context menu.  The |src_url| is passed to the search request and is
  // not used directly to fetch the image resources. The
  // |additional_query_params| are also passed to the search request as part of
  // search args.
  void SearchByImageImpl(content::RenderFrameHost* render_frame_host,
                         const GURL& src_url,
                         int thumbnail_min_size,
                         int thumbnail_max_width,
                         int thumbnail_max_height,
                         const std::string& additional_query_params,
                         bool use_side_panel,
                         bool is_image_translate);
  void SearchByImageImpl(const gfx::Image& image,
                         const gfx::Size& image_original_size,
                         const std::string& additional_query_params,
                         bool use_side_panel,
                         std::vector<lens::mojom::LatencyLogPtr> log_data);

  // Sets search args used for image translation if the current page is
  // currently being translated.
  void MaybeSetSearchArgsForImageTranslate(
      TemplateURLRef::SearchTermsArgs& search_args);

  // Triggers the Lens ping if needed, as indicated by the feature flag setting.
  // This should be called before a Lens Standalone request is initiated.
  void TriggerLensPingIfEnabled();

  // The time when we started to create the new tab page.  This time is from
  // before we created this WebContents.
  base::TimeTicks new_tab_start_time_;

  // Content restrictions, used to disable print/copy etc based on content's
  // (full-page plugins for now only) permissions.
  int content_restrictions_ = 0;

  // Stores the settings for opening a Lens search.
  struct LensSearchSettings {
    // Whether or not the search should occur in the side panel.
    bool use_side_panel;

    // The url params to open.
    content::OpenURLParams url_params;
  };

  // If Lens pinging is enabled, and a sequential Lens ping is awaiting a
  // response, this will be set.
  bool awaiting_lens_ping_response_ = false;

  // If sequential Lens pinging is enabled, this stores the Lens search settings
  // if a Lens search is ready before the Lens ping response was received.
  absl::optional<LensSearchSettings> stored_lens_search_settings_;

  // The time that the last Lens ping request was initiated.
  base::TimeTicks lens_ping_start_time_;

  base::WeakPtrFactory<CoreTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
