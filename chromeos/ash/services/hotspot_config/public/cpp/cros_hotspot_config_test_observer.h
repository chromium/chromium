// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_

#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::hotspot_config {

class CrosHotspotConfigTestObserver : public mojom::CrosHotspotConfigObserver {
 public:
  CrosHotspotConfigTestObserver();

  CrosHotspotConfigTestObserver(const CrosHotspotConfigTestObserver&) = delete;
  CrosHotspotConfigTestObserver& operator=(
      const CrosHotspotConfigTestObserver&) = delete;

  ~CrosHotspotConfigTestObserver() override;

  mojo::PendingRemote<mojom::CrosHotspotConfigObserver> GenerateRemote();

  // mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  size_t hotspot_info_changed_count() const {
    return hotspot_info_changed_count_;
  }

  mojo::Receiver<mojom::CrosHotspotConfigObserver>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::CrosHotspotConfigObserver> receiver_{this};
  size_t hotspot_info_changed_count_ = 0;
};

}  // namespace ash::hotspot_config

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_
