// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/navigation/system_entropy.mojom.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/tab_groups/tab_group_id.h"
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
#if BUILDFLAG(IS_ANDROID)
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

  NavigateParams(const NavigateParams&) = delete;
  NavigateParams& operator=(const NavigateParams&) = delete;

  NavigateParams(NavigateParams&& params);

  ~NavigateParams();

  // Copies fields from |params| struct to |nav_params| struct.
  void FillNavigateParamsFromOpenURLParams(
      const content::OpenURLParams& params);

  // The URL/referrer to be loaded. Ignored if |contents_to_insert| is non-NULL.
  GURL url;
  content::Referrer referrer;

  // The frame token of the initiator of the navigation. This is best effort: it
  // is only defined for some renderer-initiated navigations (e.g., not drag and
  // drop), and the frame with the corresponding frame token may have been
  // deleted before the navigation begins. It is defined if and only if
  // |initiator_process_id| below is.
  std::optional<blink::LocalFrameToken> initiator_frame_token;

  // ID of the renderer process of the frame host that initiated the navigation.
  // This is defined if and only if |initiator_frame_token| above is, and it is
  // only valid in conjunction with it.
  int initiator_process_id = content::ChildProcessHost::kInvalidUniqueID;

  // The origin of the initiator of the navigation.
  std::optional<url::Origin> initiator_origin;

  // The base url of the initiator of the navigation. This is only set if the
  // url is about:blank or about:srcdoc.
  std::optional<GURL> initiator_base_url;

  // The frame name to be used for the main frame.
  std::string frame_name;

  // The browser-global ID of the frame to navigate, or the default invalid
  // value for the main frame.
  content::FrameTreeNodeId frame_tree_node_id;

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
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      switch_to_singleton_tab = nullptr;

  // Output parameter.
  // The WebContents in which the navigation occurred or that was inserted.
  // Guaranteed non-NULL except for note below:
  //
  // Note: If this field is set to NULL by the caller and Navigate() creates a
  // new WebContents, this field will remain NULL and the WebContents deleted if
  // the WebContents it created is not added to a TabStripModel before
  // Navigate() returns.
  raw_ptr<content::WebContents, DanglingUntriaged>
      navigated_or_inserted_contents = nullptr;

  // [in]  The WebContents that initiated the Navigate() request if such
  //       context is necessary. Default is NULL, i.e. no context.
  // [out] If NULL, this value will be set to the selected WebContents in
  //       the originating browser prior to the operation performed by
  //       Navigate(). However, if the originating page is from a different
  //       profile (e.g. an OFF_THE_RECORD page originating from a non-OTR
  //       window), then |source_contents| is reset to NULL.
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> source_contents =
      nullptr;

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
  // If disposition is NEW_BACKGROUND_TAB, AddTabTypes::ADD_ACTIVE is
  // removed from |tabstrip_add_types| automatically.
  // If disposition is one of NEW_WINDOW, NEW_POPUP, NEW_FOREGROUND_TAB or
  // SINGLETON_TAB, then AddTabTypes::ADD_ACTIVE is automatically added to
  // |tabstrip_add_types|.
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;

  // Allows setting the opener for the case when new WebContents are created
  // (i.e. when |disposition| asks for a new tab or window).
  raw_ptr<content::RenderFrameHost> opener = nullptr;

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
  std::string app_id;

  // Specifies the desired window features if `disposition` is NEW_POPUP.
  blink::mojom::WindowFeatures window_features;

  // Determines if and how the target window should be made visible at the end
  // of the call to Navigate().
  enum WindowAction {
    // Do not show or activate the browser window after navigating.
    NO_ACTION,
    // Show and activate the browser window after navigating.
    SHOW_WINDOW,
    // Show the browser window after navigating but do not activate.
    // Note: This may cause a space / virtual desktop switch if the window is
    // being shown on a display which is currently showing a fullscreen app.
    // (crbug.com/1315749).
    SHOW_WINDOW_INACTIVE
  };
  // Default is NO_ACTION (don't show or activate the window).
  // If disposition is NEW_WINDOW or NEW_POPUP, and |window_action| is set to
  // NO_ACTION, |window_action| will be set to SHOW_WINDOW.
  WindowAction window_action = NO_ACTION;

  // Captive portal type for this browser window.
  captive_portal::CaptivePortalWindowType captive_portal_window_type =
      captive_portal::CaptivePortalWindowType::kNone;

  // Whether the browser popup is being created as a tab modal. If true,
  // `disposition` should be NEW_POPUP.
  bool is_tab_modal_popup = false;

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

#if !BUILDFLAG(IS_ANDROID)
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
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser = nullptr;

  // The group the caller would like the tab to be added to.
  std::optional<tab_groups::TabGroupId> group;

  // A bitmask of values defined in TabStripModel::AddTabTypes. Helps
  // determine where to insert a new tab and whether or not it should be
  // selected, among other properties.
  int tabstrip_add_types = AddTabTypes::ADD_ACTIVE;
#endif

  // The profile that is initiating the navigation. If there is a non-NULL
  // browser passed in via |browser|, it's profile will be used instead.
  raw_ptr<Profile> initiating_profile = nullptr;

  // Indicates whether this navigation  should replace the current
  // navigation entry.
  bool should_replace_current_entry = false;

  // Indicates whether |contents_to_insert| is being created by another window,
  // and thus can be closed via window.close(). This may be true even when
  // "noopener" was used.
  bool opened_by_another_window = false;

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

  // Indicates that the navigation must happen in a PWA window. If a PWA
  // window can't be created, the navigation will be cancelled.
  bool force_open_pwa_window = false;

  // The time when the input which led to the navigation occurred. Currently
  // only set when a link is clicked or the navigation takes place from the
  // desktop omnibox.
  base::TimeTicks input_start;

  // Indicates that the new page should have a propagated user activation.
  // This should be used when we want to pass an activation that occurred
  // outside of the page and pass it to the page as if it happened on a prior
  // page. For example, if the assistant opens a page we should treat the
  // user's interaction with the assistant as a previous user activation.
  blink::mojom::WasActivatedOption was_activated =
      blink::mojom::WasActivatedOption::kUnknown;

  // If this navigation was initiated from a link that specified the
  // hrefTranslate attribute, this contains the attribute's value (a BCP47
  // language code). Empty otherwise.
  std::string href_translate;

  // Indicates the reload type of this navigation.
  content::ReloadType reload_type = content::ReloadType::NONE;

  // Optional impression associated with this navigation. Only set on
  // navigations that originate from links with impression attributes. Used for
  // conversion measurement.
  std::optional<blink::Impression> impression;

  // True if the navigation was initiated by typing in the omnibox but the typed
  // text didn't have a scheme such as http or https (e.g. google.com), and
  // https was used as the default scheme for the navigation. This is used by
  // TypedNavigationUpgradeThrottle to determine if the navigation should be
  // observed and fall back to using http scheme if necessary.
  bool is_using_https_as_default_scheme = false;

  // True if the navigation was initiated by typing in the omnibox and the typed
  // text had an explicit http scheme.
  bool url_typed_with_http_scheme = false;

  // Indicates if the page load occurs during a non-optimal performance state.
  // This value is only suggested based upon the load context, and can be
  // overridden by other factors.
  blink::mojom::SystemEntropy suggested_system_entropy =
      blink::mojom::SystemEntropy::kNormal;

 private:
  NavigateParams();
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_H_
