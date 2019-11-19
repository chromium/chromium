// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "content/public/common/was_activated_option.mojom.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

class Browser;
class Profile;

namespace content {
class RenderFrameHost;
class WebContents;
struct OpenURLParams;
}  // namespace content

// Parameters that tell Navigate() what to do.
//
// Some basic examples:
//
// Simple Navigate to URL in current tab:
// NavigateParams params(browser, GURL("http://www.google.com/"),
//                               ui::PAGE_TRANSITION_LINK);
// Navigate(&params);
//
// Open bookmark in new background tab:
// NavigateParams params(browser, url,
//                               ui::PAGE_TRANSITION_AUTO_BOOKMARK);
// params.disposition = NEW_BACKGROUND_TAB;
// Navigate(&params);
//
// Opens a popup WebContents:
// NavigateParams params(browser, popup_contents);
// params.source_contents = source_contents;
// Navigate(&params);
//
// See browser_navigator_browsertest.cc for more examples.

// TODO(thestig): Split or ifdef out more fields that are not used on Android.
struct NavigateParams {
#if defined(OS_ANDROID)
  explicit NavigateParams(
      std::unique_ptr<content::WebContents> contents_to_insert);
#else
  NavigateParams(Browser* browser,
                 const GURL& a_url,
                 ui::PageTransition a_transition);
  NavigateParams(Browser* browser,
                 std::unique_ptr<content::WebContents> contents_to_insert);
#endif
  NavigateParams(Profile* profile,
                 const GURL& a_url,
                 ui::PageTransition a_transition);
  NavigateParams(NavigateParams&& params);
  ~NavigateParams();

  // Copies fields from |params| struct to |nav_params| struct.
  void FillNavigateParamsFromOpenURLParams(
      const content::OpenURLParams& params);

  // The URL/referrer to be loaded. Ignored if |contents_to_insert| is non-NULL.
  GURL url;
  content::Referrer referrer;

  // The origin of the initiator of the navigation.
  base::Optional<url::Origin> initiator_origin;

  // The frame name to be used for the main frame.
  std::string frame_name;

  // The browser-global ID of the frame to navigate, or
  // content::RenderFrameHost::kNoFrameTreeNodeId for the main frame.
  int frame_tree_node_id = -1;

  // Any redirect URLs that occurred for this navigation before |url|.
  // Usually empty.
  std::vector<GURL> redirect_chain;

  // The post data when the navigation uses POST.
  scoped_refptr<network::ResourceRequestBody> post_data;

  // Extra headers to add to the request for this page.  Headers are
  // represented as "<name>: <value>" and separated by \r\n.  The entire string
  // is terminated by \r\n.  May be empty if no extra headers are needed.
  std::string extra_headers;

  // Input parameter.
  // WebContents to be inserted into the target Browser's tabstrip. If NULL,
  // |url| or the homepage will be used instead. When non-NULL, Navigate()
  // assumes it has already been navigated to its intended destination and will
  // not load any URL in it (i.e. |url| is ignored). Default is NULL.
  std::unique_ptr<content::WebContents> contents_to_insert;

  // Input parameter.
  // Only used by Singleton tabs. Causes a tab-switch in addition to navigation.
  content::WebContents* switch_to_singleton_tab = nullptr;

  // Output parameter.
  // The WebContents in which the navigation occurred or that was inserted.
  // Guaranteed non-NULL except for note below:
  //
  // Note: If this field is set to NULL by the caller and Navigate() creates a
  // new WebContents, this field will remain NULL and the WebContents deleted if
  // the WebContents it created is not added to a TabStripModel before
  // Navigate() returns.
  content::WebContents* navigated_or_inserted_contents = nullptr;

  // [in]  The WebContents that initiated the Navigate() request if such
  //       context is necessary. Default is NULL, i.e. no context.
  // [out] If NULL, this value will be set to the selected WebContents in
  //       the originating browser prior to the operation performed by
  //       Navigate(). However, if the originating page is from a different
  //       profile (e.g. an OFF_THE_RECORD page originating from a non-OTR
  //       window), then |source_contents| is reset to NULL.
  content::WebContents* source_contents = nullptr;

  // The disposition requested by the navigation source. Default is
  // CURRENT_TAB. What follows is a set of coercions that happen to this value
  // when other factors are at play:
  //
  // [in]:                Condition:                        [out]:
  // NEW_BACKGROUND_TAB   target browser tabstrip is empty  NEW_FOREGROUND_TAB
  // CURRENT_TAB          "     "     "                     NEW_FOREGROUND_TAB
  // NEW_BACKGROUND_TAB   target browser is an app browser  NEW_FOREGROUND_TAB
  // OFF_THE_RECORD       target browser profile is incog.  NEW_FOREGROUND_TAB
  //
  // If disposition is NEW_BACKGROUND_TAB, TabStripModel::ADD_ACTIVE is
  // removed from |tabstrip_add_types| automatically.
  // If disposition is one of NEW_WINDOW, NEW_POPUP, NEW_FOREGROUND_TAB or
  // SINGLETON_TAB, then TabStripModel::ADD_ACTIVE is automatically added to
  // |tabstrip_add_types|.
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;

  // Allows setting the opener for the case when new WebContents are created
  // (i.e. when |disposition| asks for a new tab or window).
  content::RenderFrameHost* opener = nullptr;

  // Sets browser->is_trusted_source.
  bool trusted_source = false;

  // The transition type of the navigation.
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;

  // Whether this navigation was initiated by the renderer process.
  bool is_renderer_initiated = false;

  // The index the caller would like the tab to be positioned at in the
  // TabStrip. The actual index will be determined by the TabHandler in
  // accordance with |add_types|. The default allows the TabHandler to decide.
  int tabstrip_index = -1;

  // If non-empty, the new tab is an app tab.
  std::string extension_app_id;

  // If non-empty, specifies the desired initial position and size of the
  // window if |disposition| == NEW_POPUP.
  // TODO(beng): Figure out if this can be used to create Browser windows
  //             for other callsites that use set_override_bounds, or
  //             remove this comment.
  gfx::Rect window_bounds;

  // Determines if and how the target window should be made visible at the end
  // of the call to Navigate().
  enum WindowAction {
    // Do not show or activate the browser window after navigating.
    NO_ACTION,
    // Show and activate the browser window after navigating.
    SHOW_WINDOW,
    // Show the browser window after navigating but do not activate.
    SHOW_WINDOW_INACTIVE
  };
  // Default is NO_ACTION (don't show or activate the window).
  // If disposition is NEW_WINDOW or NEW_POPUP, and |window_action| is set to
  // NO_ACTION, |window_action| will be set to SHOW_WINDOW.
  WindowAction window_action = NO_ACTION;

  // If false then the navigation was not initiated by a user gesture.
  bool user_gesture = true;

  // What to do with the path component of the URL for singleton navigations.
  enum PathBehavior {
    // Two URLs with differing paths are different.
    RESPECT,
    // Ignore path when finding existing tab, navigate to new URL.
    IGNORE_AND_NAVIGATE,
  };
  PathBehavior path_behavior = RESPECT;

#if !defined(OS_ANDROID)
  // [in]  Specifies a Browser object where the navigation could occur or the
  //       tab could be added. Navigate() is not obliged to use this Browser if
  //       it is not compatible with the operation being performed. This can be
  //       NULL, in which case |initiating_profile| must be provided.
  // [out] Specifies the Browser object where the navigation occurred or the
  //       tab was added. Guaranteed non-NULL unless the disposition did not
  //       require a navigation, in which case this is set to NULL
  //       (SAVE_TO_DISK, IGNORE_ACTION).
  // Note: If |show_window| is set to false and a new Browser is created by
  //       Navigate(), the caller is responsible for showing it so that its
  //       window can assume responsibility for the Browser's lifetime (Browser
  //       objects are deleted when the user closes a visible browser window).
  Browser* browser = nullptr;

  // The group the caller would like the tab to be added to.
  base::Optional<TabGroupId> group;

  // A bitmask of values defined in TabStripModel::AddTabTypes. Helps
  // determine where to insert a new tab and whether or not it should be
  // selected, among other properties.
  int tabstrip_add_types = TabStripModel::ADD_ACTIVE;
#endif

  // The profile that is initiating the navigation. If there is a non-NULL
  // browser passed in via |browser|, it's profile will be used instead.
  Profile* initiating_profile = nullptr;

  // Indicates whether this navigation  should replace the current
  // navigation entry.
  bool should_replace_current_entry = false;

  // Indicates whether |contents_to_insert| is being created with a
  // window.opener.
  bool created_with_opener = false;

  // Whether or not the related navigation was started in the context menu.
  bool started_from_context_menu = false;

  // SiteInstance of the frame that initiated the navigation or null if we
  // don't know it. This should be assigned from the OpenURLParams of the
  // WebContentsDelegate::OpenURLFromTab implementation and is used to determine
  // the SiteInstance that will be used for the resulting frame in the case of
  // an about:blank or a data url navigation.
  scoped_refptr<content::SiteInstance> source_site_instance;

  // Optional URLLoaderFactory to facilitate blob URL loading.
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;

  // Indicates that the navigation should happen in an pwa window if
  // possible, i.e. if the is a PWA installed for the target URL.
  bool open_pwa_window_if_possible = false;

  // The time when the input which led to the navigation occurred. Currently
  // only set when a link is clicked or the navigation takes place from the
  // desktop omnibox.
  base::TimeTicks input_start;

  // Indicates that the new page should have a propagated user activation.
  // This should be used when we want to pass an activation that occurred
  // outside of the page and pass it to the page as if it happened on a prior
  // page. For example, if the assistant opens a page we should treat the
  // user's interaction with the assistant as a previous user activation.
  content::mojom::WasActivatedOption was_activated =
      content::mojom::WasActivatedOption::kUnknown;

  // If this navigation was initiated from a link that specified the
  // hrefTranslate attribute, this contains the attribute's value (a BCP47
  // language code). Empty otherwise.
  std::string href_translate;

  // Indicates the reload type of this navigation.
  content::ReloadType reload_type = content::ReloadType::NONE;

 private:
  NavigateParams();
  DISALLOW_COPY_AND_ASSIGN(NavigateParams);
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_
