// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/oop_local_search_service_provider.h"

#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace ash::local_search_service {

OopLocalSearchServiceProvider::OopLocalSearchServiceProvider() {
  LocalSearchServiceProvider::Set(this);
}

OopLocalSearchServiceProvider::~OopLocalSearchServiceProvider() {
  LocalSearchServiceProvider::Set(nullptr);
}

void OopLocalSearchServiceProvider::BindLocalSearchService(
    mojo::PendingReceiver<mojom::LocalSearchService> receiver) {
  content::ServiceProcessHost::Launch(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("Local Search Service")
                               .Pass());
}

}  // namespace ash::local_search_service
