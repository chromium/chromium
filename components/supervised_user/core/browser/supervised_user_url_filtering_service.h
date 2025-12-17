// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {
// Performs URL filtering workflows for supervised users, combining effects of
// subservices that define the status of these users.
class SupervisedUserUrlFilteringService : public KeyedService {
 public:
  explicit SupervisedUserUrlFilteringService(
      const SupervisedUserService& supervised_user_service);
  ~SupervisedUserUrlFilteringService() override;
  SupervisedUserUrlFilteringService(const SupervisedUserUrlFilteringService&) =
      delete;
  SupervisedUserUrlFilteringService& operator=(
      const SupervisedUserUrlFilteringService&) = delete;

  // Returns the type of web filter that is applied to the current profile.
  WebFilterType GetWebFilterType() const;

 private:
  // Provides access to legacy way of resolving URL filtering.
  raw_ref<const SupervisedUserService> supervised_user_service_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
