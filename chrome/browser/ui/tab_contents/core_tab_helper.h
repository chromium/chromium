// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
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

class SkBitmap;

using DownscaleAndEncodeBitmapCallback = base::OnceCallback<void(
    const std::vector<unsigned char>& thumbnail_data,
    const std::string& content_type,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::vector<lens::mojom::LatencyLogPtr> log_data)>;

// Per-tab class to handle functionality that is core to the operation of tabs.
// TODO(crbug.com/346044243): Delete this class.
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

  // Encodes the given image to proper image format, adds to |search_args|
  // thumbnail image content data, and emits the image bytes size. Also
  // returns the format the image was encoded to.
  // Public for testing.
  static lens::mojom::ImageFormat EncodeImageIntoSearchArgs(
      const gfx::Image& image,
      size_t& encoded_size_bytes,
      TemplateURLRef::SearchTermsArgs& search_args);

  // Downscales and encodes the image and sets the content type for the result
  // image. The resulting format will be jpeg if the image was opaque and webp
  // otherwise. Returns the vector of image bytes. Public for testing.
  static void DownscaleAndEncodeBitmap(
      const SkBitmap& bitmap,
      int thumbnail_min_area,
      int thumbnail_max_width,
      int thumbnail_max_height,
      DownscaleAndEncodeBitmapCallback callback);

  // Opens the Lens standalone experience for the image that triggered the
  // context menu. If the google lens supports opening requests in side panel,
  // then the request will open in the side panel instead of new tab, unless
  // force_open_in_new_tab is set.
  void SearchWithLens(content::RenderFrameHost* render_frame_host,
                      const GURL& src_url,
                      lens::EntryPoint entry_point,
                      bool is_image_translate,
                      bool force_open_in_new_tab);

  // Opens the Lens experience for an `image`, which will be resized if needed.
  // If the search engine supports opening requests in side panel, then the
  // request will open in the side panel instead of a new tab, unless
  // force_open_in_new_tab is set.
  void SearchWithLens(const gfx::Image& image,
                      lens::EntryPoint entry_point,
                      bool force_open_in_new_tab);

  // Performs an image search for the image that triggered the context menu. The
  // `src_url` is passed to the search request and is not used directly to fetch
  // the image resources. If the search engine supports opening requests in side
  // panel, then the request will open in the side panel instead of a new tab.
  void SearchByImage(content::RenderFrameHost* render_frame_host,
                     const GURL& src_url);

  // Same as above, with ability to specify that the image should be translated.
  void SearchByImage(content::RenderFrameHost* render_frame_host,
                     const GURL& src_url,
                     bool is_image_translate);

  // Performs an image search for the provided `image`, which will be resized if
  // needed. If the search engine supports opening requests in side panel, then
  // the request will open in side panel instead of a new tab.
  void SearchByImage(const gfx::Image& image);

  void set_new_tab_start_time(const base::TimeTicks& time) {
    new_tab_start_time_ = time;
  }

  base::TimeTicks new_tab_start_time() const { return new_tab_start_time_; }
  int content_restrictions() const { return content_restrictions_; }

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

  // Asynchronously downscales and encodes the image from the context node
  // before issuing an image search request for the image.
  void DoSearchByImageWithBitmap(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      const GURL& src_url,
      const std::string& additional_query_params,
      bool use_side_panel,
      bool is_image_translate,
      int thumbnail_min_area,
      int thumbnail_max_width,
      int thumbnail_max_height,
      const SkBitmap& bitmap);

  void DoSearchByImage(const GURL& src_url,
                       const std::string& additional_query_params,
                       bool use_side_panel,
                       bool is_image_translate,
                       const std::vector<unsigned char>& thumbnail_data,
                       const std::string& content_type,
                       const gfx::Size& original_size,
                       const gfx::Size& downscaled_size,
                       const std::vector<lens::mojom::LatencyLogPtr> log_data);

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
  // a new tab.
  void PostContentToURL(TemplateURLRef::PostContent post_content,
                        GURL url,
                        bool use_side_panel);

  // Creates a thumbnail to POST to search engine for the image that triggered
  // the context menu.  The |src_url| is passed to the search request and is
  // not used directly to fetch the image resources. The
  // |additional_query_params| are also passed to the search request as part of
  // search args.
  void SearchByImageImpl(content::RenderFrameHost* render_frame_host,
                         const GURL& src_url,
                         int thumbnail_min_area,
                         int thumbnail_max_width,
                         int thumbnail_max_height,
                         const std::string& additional_query_params,
                         bool use_side_panel,
                         bool is_image_translate);

  // Searches the `original_image`, which will be downscaled if needed.
  void SearchByImageImpl(const gfx::Image& original_image,
                         const std::string& additional_query_params,
                         bool use_side_panel);

  // Sets search args used for image translation if the current page is
  // currently being translated.
  void MaybeSetSearchArgsForImageTranslate(
      TemplateURLRef::SearchTermsArgs& search_args);

  // The time when we started to create the new tab page.  This time is from
  // before we created this WebContents.
  base::TimeTicks new_tab_start_time_;

  // Content restrictions, used to disable print/copy etc based on content's
  // (full-page plugins for now only) permissions.
  int content_restrictions_ = 0;

  base::WeakPtrFactory<CoreTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
