// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_params_utils.h"

#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/captive_portal/core/buildflags.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

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

  if (params->post_data) {
    load_url_params.load_type =
        content::NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params->post_data;
  }
  return load_url_params;
}

#if !BUILDFLAG(IS_ANDROID)
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
    navigation_ui_data->set_navigation_initiated_from_sync(
        params->navigation_initiated_from_sync);
    load_url_params.navigation_ui_data = std::move(navigation_ui_data);
  }

  return load_url_params;
}
#endif
