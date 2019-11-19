// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_
#define COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace error_page {

struct ErrorPageParams;

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
    base::DictionaryValue strings;

    bool is_offline_error = false;
    bool reload_button_shown = false;
    bool show_cached_copy_button_shown = false;
    bool download_button_shown = false;
    bool offline_content_feature_enabled = false;
    bool auto_fetch_allowed = false;
  };

  // Returns a |PageState| that describes the elements that should be shown on
  // on HTTP errors, like 404 or connection reset.
  static PageState GetPageState(
      int error_code,
      const std::string& error_domain,
      const GURL& failed_url,
      bool is_post,
      bool stale_copy_in_cache,
      bool can_show_network_diagnostics_dialog,
      bool is_incognito,
      bool offline_content_feature_enabled,
      bool auto_fetch_feature_enabled,
      const std::string& locale,
      std::unique_ptr<error_page::ErrorPageParams> params);

  // Returns a description of the encountered error.
  static base::string16 GetErrorDetails(const std::string& error_domain,
                                        int error_code,
                                        bool is_post);

  // Returns true if an error page exists for the specified parameters.
  static bool HasStrings(const std::string& error_domain, int error_code);
 private:

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalizedError);
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_LOCALIZED_ERROR_H_
