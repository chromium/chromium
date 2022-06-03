// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_client_adapter.h"

#include "components/arc/arc_util.h"
#include "components/arc/session/arc_container_client_adapter.h"
#include "components/arc/session/arc_vm_client_adapter.h"

namespace arc {

ArcClientAdapter::ArcClientAdapter() = default;
ArcClientAdapter::~ArcClientAdapter() = default;

void ArcClientAdapter::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcClientAdapter::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
std::unique_ptr<ArcClientAdapter> ArcClientAdapter::Create() {
  return IsArcVmEnabled() ? CreateArcVmClientAdapter()
                          : CreateArcContainerClientAdapter();
}

}  // namespace arc
