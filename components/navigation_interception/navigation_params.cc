// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/navigation_params.h"

namespace navigation_interception {

NavigationParams::NavigationParams(const GURL& url,
                                   const content::Referrer& referrer,
                                   bool has_user_gesture,
                                   bool is_post,
                                   ui::PageTransition transition_type,
                                   bool is_redirect,
                                   bool is_external_protocol,
                                   bool is_main_frame,
                                   bool is_renderer_initiated,
                                   const GURL& base_url_for_data_url)
    : url_(url),
      referrer_(referrer),
      has_user_gesture_(has_user_gesture),
      is_post_(is_post),
      transition_type_(transition_type),
      is_redirect_(is_redirect),
      is_external_protocol_(is_external_protocol),
      is_main_frame_(is_main_frame),
      is_renderer_initiated_(is_renderer_initiated),
      base_url_for_data_url_(base_url_for_data_url) {}

NavigationParams::~NavigationParams() = default;

NavigationParams::NavigationParams(const NavigationParams&) = default;

}  // namespace navigation_interception

