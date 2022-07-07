// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_provider.h"

#include "chromeos/ash/components/local_search_service/local_search_service.h"

namespace chromeos {
namespace local_search_service {

namespace {

LocalSearchServiceProvider* g_provider = nullptr;

}  // namespace

void LocalSearchServiceProvider::Set(LocalSearchServiceProvider* provider) {
  g_provider = provider;
}

LocalSearchServiceProvider* LocalSearchServiceProvider::Get() {
  return g_provider;
}

}  // namespace local_search_service
}  // namespace chromeos
