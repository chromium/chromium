// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_

#include "base/callback.h"
#include "base/supports_user_data.h"

namespace content {
class BrowserContext;
}  // namespace content

class GURL;

// Stores per-profile configuration data for side search.
class SideSearchConfig : public base::SupportsUserData::Data {
 public:
  using URLTestConditionCallback = base::RepeatingCallback<bool(const GURL&)>;

  SideSearchConfig();
  SideSearchConfig(const SideSearchConfig&) = delete;
  SideSearchConfig& operator=(const SideSearchConfig&) = delete;
  ~SideSearchConfig() override;

  // Gets the instance of the config for `context`.
  static SideSearchConfig* Get(content::BrowserContext* context);

  // Returns whether a `url` in the side panel should be allowed to commit in
  // the side panel or if it should be redirected to the content frame.
  bool ShouldNavigateInSidePanel(const GURL& url);
  void SetShouldNavigateInSidePanelCalback(URLTestConditionCallback callback);

  // Returns whether the side panel can be shown for the `url`. This is used to
  // avoid having the side panel on pages on which it doesn't make sense to have
  // it appear (e.g. NTP).
  bool CanShowSidePanelForURL(const GURL& url);
  void SetCanShowSidePanelForURLCallback(URLTestConditionCallback callback);

  // Gets and sets the bit that determines whether or not the SRP is available.
  // TODO(tluk): Move the code that tests for availability into this class.
  bool is_side_panel_srp_available() { return is_side_panel_srp_available_; }
  void set_is_side_panel_srp_available(bool is_side_panel_srp_available) {
    is_side_panel_srp_available_ = is_side_panel_srp_available;
  }

 private:
  // Whether or not the service providing the SRP for the side panel is
  // available or not.
  bool is_side_panel_srp_available_ = false;

  URLTestConditionCallback should_navigate_in_side_panel_callback_;
  URLTestConditionCallback can_show_side_panel_for_url_callback_;
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_
