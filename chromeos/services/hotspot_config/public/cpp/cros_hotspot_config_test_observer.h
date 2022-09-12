// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_
#define CHROMEOS_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_

#include <string>

#include "chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace hotspot_config {

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
  void OnHotspotStateFailed(const std::string& error_code) override;

  size_t hotspot_info_changed_count() const {
    return hotspot_info_changed_count_;
  }
  size_t hotspot_state_failed_count() const {
    return hotspot_state_failed_count_;
  }
  const std::string& last_hotspot_failed_error() const {
    return last_hotspot_failed_error_;
  }

  mojo::Receiver<mojom::CrosHotspotConfigObserver>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::CrosHotspotConfigObserver> receiver_{this};
  size_t hotspot_info_changed_count_ = 0;
  size_t hotspot_state_failed_count_ = 0;
  std::string last_hotspot_failed_error_;
};

}  // namespace hotspot_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_TEST_OBSERVER_H_