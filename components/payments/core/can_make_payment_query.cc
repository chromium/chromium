// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/can_make_payment_query.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/payments/core/features.h"
#include "url/gurl.h"

namespace payments {

CanMakePaymentQuery::CanMakePaymentQuery() {}

CanMakePaymentQuery::~CanMakePaymentQuery() {}

bool CanMakePaymentQuery::CanQuery(
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query,
    bool per_method_quota) {
  // Check both with and without per-method quota, so that both queries are
  // recorded in case if two different tabs of the same website run with and
  // without the origin trial.
  bool can_query_with_per_method_quota =
      CanQueryWithPerMethodQuota(top_level_origin, frame_origin, query);

  bool can_query_without_per_method_quota =
      CanQueryWithoutPerMethodQuota(top_level_origin, frame_origin, query);

  if (per_method_quota ||
      base::FeatureList::IsEnabled(
          features::kWebPaymentsPerMethodCanMakePaymentQuota)) {
    return can_query_with_per_method_quota;
  }

  return can_query_without_per_method_quota;
}

bool CanMakePaymentQuery::CanQueryWithPerMethodQuota(
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  bool can_query = true;
  for (const auto& method_and_params : query) {
    std::string method = method_and_params.first;
    std::set<std::string> params = method_and_params.second;

    const std::string id =
        frame_origin.spec() + ":" + top_level_origin.spec() + ":" + method;

    auto it = per_method_queries_.find(id);
    if (it == per_method_queries_.end()) {
      auto timer = std::make_unique<base::OneShotTimer>();
      timer->Start(FROM_HERE, base::TimeDelta::FromMinutes(30),
                   base::BindOnce(
                       &CanMakePaymentQuery::ExpireQuotaForFrameOriginAndMethod,
                       weak_ptr_factory_.GetWeakPtr(), id));
      timers_.insert(std::make_pair(id, std::move(timer)));
      per_method_queries_.insert(std::make_pair(id, params));
      continue;
    }

    can_query &= it->second == params;
  }

  return can_query;
}

bool CanMakePaymentQuery::CanQueryWithoutPerMethodQuota(
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  const std::string id = frame_origin.spec() + ":" + top_level_origin.spec();

  const auto& it = queries_.find(id);
  if (it == queries_.end()) {
    auto timer = std::make_unique<base::OneShotTimer>();
    timer->Start(FROM_HERE, base::TimeDelta::FromMinutes(30),
                 base::BindOnce(&CanMakePaymentQuery::ExpireQuotaForFrameOrigin,
                                weak_ptr_factory_.GetWeakPtr(), id));
    timers_.insert(std::make_pair(id, std::move(timer)));
    queries_.insert(std::make_pair(id, query));
    return true;
  }

  return it->second == query;
}

void CanMakePaymentQuery::ExpireQuotaForFrameOrigin(const std::string& id) {
  timers_.erase(id);
  queries_.erase(id);
}

void CanMakePaymentQuery::ExpireQuotaForFrameOriginAndMethod(
    const std::string& id) {
  timers_.erase(id);
  per_method_queries_.erase(id);
}

}  // namespace payments
