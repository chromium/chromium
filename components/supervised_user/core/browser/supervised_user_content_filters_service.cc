// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"

namespace supervised_user {
SupervisedUserContentFiltersService::SupervisedUserContentFiltersService() =
    default;
SupervisedUserContentFiltersService::~SupervisedUserContentFiltersService() =
    default;

void SupervisedUserContentFiltersService::SetBrowserFiltersEnabled(
    bool enabled) {
  browser_filters_enabled_ = enabled;
  Notify();
}

void SupervisedUserContentFiltersService::SetSearchFiltersEnabled(
    bool enabled) {
  search_filters_enabled_ = enabled;
  Notify();
}

void SupervisedUserContentFiltersService::Notify() {
  State state;
  state.incognito_disabled =
      browser_filters_enabled_ || search_filters_enabled_;
  state.safe_sites_enabled = browser_filters_enabled_;
  state.safe_search_enabled = search_filters_enabled_;

  callback_list_.Notify(std::move(state));
}

base::CallbackListSubscription
SupervisedUserContentFiltersService::SubscribeForContentFiltersStateChange(
    Callback callback) {
  return callback_list_.Add(std::move(callback));
}

}  // namespace supervised_user
