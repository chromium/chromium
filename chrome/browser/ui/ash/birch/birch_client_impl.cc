// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_client_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"

namespace ash {

BirchClientImpl::BirchClientImpl(Profile* profile) : profile_(profile) {}

BirchClientImpl::~BirchClientImpl() = default;

void BirchClientImpl::RequestBirchDataFetch() {
  BirchKeyedServiceFactory::GetInstance()
      ->GetService(profile_)
      ->RequestBirchDataFetch();
}

}  // namespace ash
