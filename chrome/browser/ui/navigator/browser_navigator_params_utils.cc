// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigator/browser_navigator_params_utils.h"

#include <algorithm>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    NavigateParams* params) {
  content::NavigationController::LoadURLParams load_url_params(params->url);
  load_url_params.initiator_frame_token = params->initiator_frame_token;
  load_url_params.initiator_process_id = params->initiator_process_id;
  load_url_params.initiator_origin = params->initiator_origin;
  load_url_params.initiator_base_url = params->initiator_base_url;
  load_url_params.source_site_instance = params->source_site_instance;
  load_url_params.referrer = params->referrer;
  load_url_params.frame_name = params->frame_name;
  load_url_params.frame_tree_node_id = params->frame_tree_node_id;
  load_url_params.redirect_chain = params->redirect_chain;
  load_url_params.transition_type = params->transition;
  load_url_params.extra_headers = params->extra_headers;
  load_url_params.should_replace_current_entry =
      params->should_replace_current_entry;
  load_url_params.is_renderer_initiated = params->is_renderer_initiated;
  load_url_params.started_from_context_menu = params->started_from_context_menu;
  load_url_params.has_user_gesture = params->user_gesture;
  load_url_params.blob_url_loader_factory = params->blob_url_loader_factory;
  load_url_params.input_start = params->input_start;
  load_url_params.was_activated = params->was_activated;
  load_url_params.href_translate = params->href_translate;
  load_url_params.reload_type = params->reload_type;
  load_url_params.impression = params->impression;
  load_url_params.internal_scroll_to_text_fragment =
      params->internal_scroll_to_text_fragment;
  load_url_params.started_by_ad = params->started_by_ad;

  if (params->post_data) {
    load_url_params.load_type =
        content::NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params->post_data;
  }
  return load_url_params;
}

content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    content::WebContents* target_contents,
    NavigateParams* params) {
  auto load_url_params = LoadURLParamsFromNavigateParams(params);

  // |frame_tree_node_id| is invalid for main frame navigations.
  if (params->frame_tree_node_id.is_null()) {
    bool force_no_https_upgrade =
        params->url_typed_with_http_scheme ||
        params->captive_portal_window_type !=
            captive_portal::CaptivePortalWindowType::kNone;
    std::unique_ptr<ChromeNavigationUIData> navigation_ui_data =
        ChromeNavigationUIData::CreateForMainFrameNavigation(
            target_contents, params->is_using_https_as_default_scheme,
            force_no_https_upgrade);
#if !BUILDFLAG(IS_ANDROID)
    // This field is not available on Android.
    navigation_ui_data->set_navigation_initiated_from_sync(
        params->navigation_initiated_from_sync);
#endif
    load_url_params.navigation_ui_data = std::move(navigation_ui_data);
  }

  return load_url_params;
}

namespace {

// Returns true if two URLs are equal after taking |replacements| into account.
bool CompareURLsWithReplacements(const GURL& url,
                                 const GURL& other,
                                 const GURL::Replacements& replacements,
                                 TemplateURLService* template_url_service) {
  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL other_replaced = other.ReplaceComponents(replacements);
  AutocompleteInput input;
  return AutocompleteMatch::GURLToStrippedGURL(
             url_replaced, input, template_url_service, std::u16string(),
             /*keep_search_intent_params=*/false) ==
         AutocompleteMatch::GURLToStrippedGURL(
             other_replaced, input, template_url_service, std::u16string(),
             /*keep_search_intent_params=*/false);
}

}  // namespace

int GetIndexOfExistingTabMatchingURL(BrowserWindowInterface* browser,
                                     const NavigateParams& params) {
  if (params.disposition != WindowOpenDisposition::SINGLETON_TAB &&
      params.disposition != WindowOpenDisposition::SWITCH_TO_TAB) {
    return -1;
  }

  // In case the URL was rewritten by the BrowserURLHandler we need to ensure
  // that we do not open another URL that will get redirected to the rewritten
  // URL.
  const bool target_is_view_source =
      params.url.SchemeIs(content::kViewSourceScheme);
  GURL rewritten_url(params.url);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url, browser->GetProfile());

  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(browser->GetProfile());

  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return -1;
  }

  // If there are several matches: prefer the active tab by starting there.
  int start_index = std::max(0, tab_list->GetActiveIndex());
  int tab_count = tab_list->GetTabCount();
  for (int i = 0; i < tab_count; ++i) {
    int tab_index = (start_index + i) % tab_count;
    tabs::TabInterface* tab_interface = tab_list->GetTab(tab_index);
    if (!tab_interface) {
      continue;
    }

    content::WebContents* tab = tab_interface->GetContents();
    if (!tab) {
      continue;
    }

    GURL tab_url = tab->GetVisibleURL();

    // RewriteURLIfNecessary removes the "view-source:" scheme which could lead
    // to incorrect matching, so ensure that the target and the candidate are
    // either both view-source:, or neither is.
    if (tab_url.SchemeIs(content::kViewSourceScheme) != target_is_view_source) {
      continue;
    }

    GURL rewritten_tab_url = tab_url;
    content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
        &rewritten_tab_url, browser->GetProfile());

    GURL::Replacements replacements;
    replacements.ClearRef();
    if (params.path_behavior == NavigateParams::IGNORE_AND_NAVIGATE) {
      replacements.ClearPath();
      replacements.ClearQuery();
    }

    if (CompareURLsWithReplacements(tab_url, params.url, replacements,
                                    turl_service) ||
        CompareURLsWithReplacements(rewritten_tab_url, rewritten_url,
                                    replacements, turl_service)) {
      return tab_index;
    }
  }

  return -1;
}

std::pair<BrowserWindowInterface*, int> GetIndexAndBrowserOfMatchingTab(
    Profile* profile,
    const NavigateParams& params) {
  BrowserWindowInterface* browser_of_existing_tab = nullptr;
  int idx = -1;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // When tab switching, only look at same profile and anonymity level.
        if (profile == browser->GetProfile() && !browser->IsDeleteScheduled()) {
          int index = GetIndexOfExistingTabMatchingURL(browser, params);
          if (index >= 0) {
            browser_of_existing_tab = browser;
            idx = index;
            return false;  // stop iterating
          }
        }
        return true;  // continue iterating
      });
  return {browser_of_existing_tab, idx};
}
