// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/lens/lens_entrypoints.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

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

  // Open the Lens standalone experience for the image that triggered the
  // context menu. If |use_side_panel| is true, the page will be opened in a
  // side panel rather than a new tab.
  void SearchWithLensInNewTab(content::RenderFrameHost* render_frame_host,
                              const GURL& src_url,
                              lens::EntryPoint entry_point,
                              bool use_side_panel);

  // Open the Lens experience for an image. Used for sending the bitmap selected
  // via Lens Region Search. |image_original_size| is specified in case of
  // resizing that happens prior to passing the image to CoreTabHelper. If
  // |use_side_panel| is true, the page will be opened in a side panel rather
  // than a new tab.
  void SearchWithLensInNewTab(gfx::Image image,
                              const gfx::Size& image_original_size,
                              lens::EntryPoint entry_point,
                              bool use_side_panel);

  // Perform an image search for the image that triggered the context menu.  The
  // |src_url| is passed to the search request and is not used directly to fetch
  // the image resources.
  void SearchByImageInNewTab(content::RenderFrameHost* render_frame_host,
                             const GURL& src_url);

  // Performs an image search for the provided image.
  void SearchByImageInNewTab(const gfx::Image& image,
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
  void OnVisibilityChanged(content::Visibility visibility) override;
  void NavigationEntriesDeleted() override;
  void OnWebContentsFocused(content::RenderWidgetHost*) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost*) override;

  void DoSearchByImageInNewTab(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      const GURL& src_url,
      const std::string& additional_query_params,
      bool use_side_panel,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& original_size,
      const std::string& image_extension);

  // Posts the bytes and content type to the specified URL If |use_side_panel|
  // is true, the content will open in a side panel, otherwise it will open in
  // a new tab.
  void PostContentToURL(TemplateURLRef::PostContent post_content,
                        GURL url,
                        bool use_side_panel);

  // Create a thumbnail to POST to search engine for the image that triggered
  // the context menu.  The |src_url| is passed to the search request and is
  // not used directly to fetch the image resources. The
  // |additional_query_params| are also passed to the search request as part of
  // search args.
  void SearchByImageInNewTabImpl(content::RenderFrameHost* render_frame_host,
                                 const GURL& src_url,
                                 int thumbnail_min_size,
                                 int thumbnail_max_width,
                                 int thumbnail_max_height,
                                 const std::string& additional_query_params,
                                 bool use_side_panel);
  void SearchByImageInNewTabImpl(const gfx::Image& image,
                                 const gfx::Size& image_original_size,
                                 const std::string& additional_query_params,
                                 bool use_side_panel);

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
