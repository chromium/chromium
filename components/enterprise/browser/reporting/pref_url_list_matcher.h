// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PREF_URL_LIST_MATCHER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PREF_URL_LIST_MATCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"

class GURL;

namespace enterprise_reporting {

class PrefURLListMatcher {
 public:
  explicit PrefURLListMatcher(PrefService* pref_service, const char* pref_name);
  PrefURLListMatcher(const PrefURLListMatcher&) = delete;
  PrefURLListMatcher& operator=(const PrefURLListMatcher&) = delete;
  ~PrefURLListMatcher();

  void OnPrefUpdated();

  std::optional<std::string> GetMatchedURL(const GURL& url) const;

 private:
  raw_ptr<PrefService> pref_service_ = nullptr;
  const char* pref_name_ = nullptr;
  PrefChangeRegistrar pref_change_;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  std::vector<size_t> path_length_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PREF_URL_LIST_MATCHER_H_
