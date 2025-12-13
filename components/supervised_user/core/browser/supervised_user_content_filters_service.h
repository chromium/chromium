// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUM_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_CONTENT_FILTERS_SERVICE_H_
#define CHROMIUM_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_CONTENT_FILTERS_SERVICE_H_

#include "base/callback_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace supervised_user {
// Translates input from the state of device content filters to output state of
// supervised user features. Typically is written from the SupervisedUserService
// and writes to the SupervisedUserPrefStore.
class SupervisedUserContentFiltersService : public KeyedService {
 public:
  // Logical feature state list derived from the configuration of filters,
  // that probably should be mapped to preferences that configure the behavior
  // of the browser.
  struct State {
    bool incognito_disabled = false;
    bool safe_sites_enabled = false;
    bool safe_search_enabled = false;
  };

  using CallbackType = void(State);
  using Callback = base::RepeatingCallback<CallbackType>;
  using CallbackList = base::RepeatingCallbackList<CallbackType>;

  SupervisedUserContentFiltersService();
  SupervisedUserContentFiltersService(
      const SupervisedUserContentFiltersService&) = delete;
  SupervisedUserContentFiltersService& operator=(
      const SupervisedUserContentFiltersService&) = delete;
  ~SupervisedUserContentFiltersService() override;

  // Flips the state of respective content filters.
  void SetBrowserFiltersEnabled(bool enabled);
  void SetSearchFiltersEnabled(bool enabled);

  // Each time a filter is changed, subscriber will be notified with the current
  // state of features.
  base::CallbackListSubscription SubscribeForContentFiltersStateChange(
      Callback callback);

 private:
  // Notifies observers of changes to filters, sending them a State instance.
  void Notify();

  bool browser_filters_enabled_ = false;
  bool search_filters_enabled_ = false;

  CallbackList callback_list_;
};
}  // namespace supervised_user

#endif  // CHROMIUM_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_CONTENT_FILTERS_SERVICE_H_
