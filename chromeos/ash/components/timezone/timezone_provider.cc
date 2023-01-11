// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/timezone/timezone_provider.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

TimeZoneProvider::TimeZoneProvider(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& url)
    : shared_url_loader_factory_(std::move(factory)), url_(url) {}

TimeZoneProvider::~TimeZoneProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TimeZoneProvider::RequestTimezone(
    const Geoposition& position,
    base::TimeDelta timeout,
    TimeZoneRequest::TimeZoneResponseCallback callback) {
  TimeZoneRequest* request(
      new TimeZoneRequest(shared_url_loader_factory_, url_, position, timeout));
  requests_.push_back(base::WrapUnique(request));

  // TimeZoneProvider owns all requests. It is safe to pass unretained "this"
  // because destruction of TimeZoneProvider cancels all requests.
  TimeZoneRequest::TimeZoneResponseCallback callback_tmp =
      base::BindOnce(&TimeZoneProvider::OnTimezoneResponse,
                     base::Unretained(this), request, std::move(callback));
  request->MakeRequest(std::move(callback_tmp));
}

void TimeZoneProvider::OnTimezoneResponse(
    TimeZoneRequest* request,
    TimeZoneRequest::TimeZoneResponseCallback callback,
    std::unique_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  auto position = base::ranges::find(requests_, request,
                                     &std::unique_ptr<TimeZoneRequest>::get);
  DCHECK(position != requests_.end());
  if (position != requests_.end()) {
    std::swap(*position, *requests_.rbegin());
    requests_.resize(requests_.size() - 1);
  }

  std::move(callback).Run(std::move(timezone), server_error);
}

}  // namespace ash
