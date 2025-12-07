// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_app.h"

namespace apps {

PreferredApp::PreferredApp(IntentFilterPtr intent_filter,
                           const std::string& app_id)
    : intent_filter(std::move(intent_filter)), app_id(app_id) {}

PreferredApp::~PreferredApp() = default;

bool PreferredApp::operator==(const PreferredApp& other) const {
  return *intent_filter == *other.intent_filter && app_id == other.app_id;
}

std::unique_ptr<PreferredApp> PreferredApp::Clone() const {
  return std::make_unique<PreferredApp>(intent_filter->Clone(), app_id);
}

PreferredApps ClonePreferredApps(const PreferredApps& preferred_apps) {
  PreferredApps ret;
  ret.reserve(preferred_apps.size());
  for (const auto& preferred_app : preferred_apps) {
    ret.push_back(preferred_app->Clone());
  }
  return ret;
}

bool IsEqual(const PreferredApps& source, const PreferredApps& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace apps
