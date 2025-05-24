// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/has_enrolled_instrument_query.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/payments/core/features.h"
#include "url/gurl.h"

namespace payments {

HasEnrolledInstrumentQuery::HasEnrolledInstrumentQuery() = default;

HasEnrolledInstrumentQuery::~HasEnrolledInstrumentQuery() = default;

bool HasEnrolledInstrumentQuery::CanQuery(
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  const std::string id = frame_origin.spec() + ":" + top_level_origin.spec();

  const auto& it = queries_.find(id);
  if (it == queries_.end()) {
    auto timer = std::make_unique<base::OneShotTimer>();
    timer->Start(
        FROM_HERE, base::Minutes(30),
        base::BindOnce(&HasEnrolledInstrumentQuery::ExpireQuotaForFrameOrigin,
                       weak_ptr_factory_.GetWeakPtr(), id));
    timers_.insert(std::make_pair(id, std::move(timer)));
    queries_.insert(std::make_pair(id, query));
    return true;
  }

  return it->second == query;
}

void HasEnrolledInstrumentQuery::Shutdown() {
  // OneShotTimer cancels the timer when it is destroyed.
  timers_.clear();
}

void HasEnrolledInstrumentQuery::ExpireQuotaForFrameOrigin(
    const std::string& id) {
  timers_.erase(id);
  queries_.erase(id);
}

}  // namespace payments
