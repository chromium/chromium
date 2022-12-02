// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_
#define COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "url/gurl.h"

namespace error_page {

// If this property set to true, override the default error page.
static constexpr const char kOverrideErrorPage[] = "override_error_page";

// Set to true when the device may be in a portal state. Will be used to show a
// suggestion in the error page where appropriate.
static constexpr const char kIsPortalStateKey[] = "is_portal_state";

class LocalizedError {
 public:
  // Information about elements shown on the error page.
  struct PageState {
    PageState();
    ~PageState();
    PageState(const PageState& other) = delete;
    PageState(PageState&& other);
    PageState& operator=(PageState&& other);

    // Strings used within the error page HTML/JS.
    base::Value::Dict strings;

    bool is_offline_error = false;
    bool reload_button_shown = false;
    bool download_button_shown = false;
    bool offline_content_feature_enabled = false;
    bool auto_fetch_allowed = false;
  };

  LocalizedError() = delete;
  LocalizedError(const LocalizedError&) = delete;
  LocalizedError& operator=(const LocalizedError&) = delete;

  // Returns a |PageState| that describes the elements that should be shown on
  // on HTTP errors, like 404 or connection reset.
  // |is_kiosk_mode| whether device is currently in the Kiosk session mode.
  static PageState GetPageState(int error_code,
                                const std::string& error_domain,
                                const GURL& failed_url,
                                bool is_post,
                                bool is_secure_dns_network_error,
                                bool stale_copy_in_cache,
                                bool can_show_network_diagnostics_dialog,
                                bool is_incognito,
                                bool offline_content_feature_enabled,
                                bool auto_fetch_feature_enabled,
                                bool is_kiosk_mode,
                                const std::string& locale,
                                bool is_blocked_by_extension,
                                const base::Value::Dict* error_page_params);

  // Returns a |PageState| that describes the elements that should be shown on
  // when default offline page is shown.
  static PageState GetPageStateForOverriddenErrorPage(
      base::Value::Dict string_dict,
      int error_code,
      const std::string& error_domain,
      const GURL& failed_url,
      const std::string& locale);

  // Returns a description of the encountered error.
  static std::u16string GetErrorDetails(const std::string& error_domain,
                                        int error_code,
                                        bool is_secure_dns_network_error,
                                        bool is_post);

  // Returns true if an error page exists for the specified parameters.
  static bool HasStrings(const std::string& error_domain, int error_code);

  static bool IsOfflineError(const std::string& error_domain, int error_code);
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_
