// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "components/keyed_service/core/keyed_service.h"

#include <memory>
#include <string>

namespace ash::nearby::presence {

class NearbyPresenceServiceImpl : public NearbyPresenceService,
                                  public KeyedService {
 public:
  NearbyPresenceServiceImpl();
  NearbyPresenceServiceImpl(const NearbyPresenceServiceImpl&) = delete;
  NearbyPresenceServiceImpl& operator=(const NearbyPresenceServiceImpl&) =
      delete;
  ~NearbyPresenceServiceImpl() override;

  // NearbyPresenceService:
  std::unique_ptr<ScanSession> StartScan(ScanFilter scan_filter,
                                         ScanDelegate* scan_delegate) override;

 private:
  // KeyedService:
  void Shutdown() override;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_
