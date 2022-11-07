// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_params.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;

#if BUILDFLAG(IS_ANDROID)
NavigateParams::NavigateParams(std::unique_ptr<WebContents> contents_to_insert)
    : contents_to_insert(std::move(contents_to_insert)) {}
#else
NavigateParams::NavigateParams(Browser* a_browser,
                               const GURL& a_url,
                               ui::PageTransition a_transition)
    : url(a_url), transition(a_transition), browser(a_browser) {}

NavigateParams::NavigateParams(Browser* a_browser,
                               std::unique_ptr<WebContents> contents_to_insert)
    : contents_to_insert(std::move(contents_to_insert)), browser(a_browser) {}
#endif  // BUILDFLAG(IS_ANDROID)

NavigateParams::NavigateParams(Profile* a_profile,
                               const GURL& a_url,
                               ui::PageTransition a_transition)
    : url(a_url),
      disposition(WindowOpenDisposition::NEW_FOREGROUND_TAB),
      transition(a_transition),
      window_action(SHOW_WINDOW),
      initiating_profile(a_profile) {}

NavigateParams::NavigateParams(NavigateParams&&) = default;

NavigateParams::~NavigateParams() {}

void NavigateParams::FillNavigateParamsFromOpenURLParams(
    const content::OpenURLParams& params) {
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  this->initiator_frame_token = params.initiator_frame_token;
  this->initiator_process_id = params.initiator_process_id;
  this->initiator_origin = params.initiator_origin;
  this->referrer = params.referrer;
  this->reload_type = params.reload_type;
  this->source_site_instance = params.source_site_instance;
  if (params.source_site_instance) {
    this->initiating_profile =
        static_cast<Profile*>(params.source_site_instance->GetBrowserContext());
  }
  this->source_contents = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(params.source_render_process_id,
                                       params.source_render_frame_id));
  this->frame_tree_node_id = params.frame_tree_node_id;
  this->redirect_chain = params.redirect_chain;
  this->extra_headers = params.extra_headers;
  this->disposition = params.disposition;
  this->trusted_source = false;
  this->is_renderer_initiated = params.is_renderer_initiated;
  this->should_replace_current_entry = params.should_replace_current_entry;
  this->post_data = params.post_data;
  this->started_from_context_menu = params.started_from_context_menu;
  this->open_pwa_window_if_possible = params.open_app_window_if_possible;
  this->user_gesture = params.user_gesture;
  this->blob_url_loader_factory = params.blob_url_loader_factory;
  this->href_translate = params.href_translate;
  this->impression = params.impression;

  // Implementation notes:
  //   The following NavigateParams don't have an equivalent in OpenURLParams:
  //     browser
  //     contents_to_insert
  //     opened_by_another_window
  //     extension_app_id
  //     frame_name
  //     group
  //     input_start
  //     navigated_or_inserted_contents
  //     opener
  //     path_behavior
  //     switch_to_singleton_tab
  //     tabstrip_add_types
  //     tabstrip_index
  //     was_activated
  //     window_action
  //     window_bounds
  //
  //   The following OpenURLParams don't have an equivalent in NavigateParams:
  //     triggering_event_info
}
