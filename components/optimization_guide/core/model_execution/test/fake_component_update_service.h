// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_COMPONENT_UPDATE_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_COMPONENT_UPDATE_SERVICE_H_

#include <string>

#include "base/observer_list.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/crx_update_item.h"

namespace optimization_guide {

class FakeComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  FakeComponentUpdateService();
  ~FakeComponentUpdateService() override;

  // component_updater::MockComponentUpdateService:
  void AddObserver(component_updater::ServiceObserver* observer) override;
  void RemoveObserver(component_updater::ServiceObserver* observer) override;

  void SendUpdate(const component_updater::CrxUpdateItem& item);
  bool HasObserver() const { return !observer_list_.empty(); }

 private:
  base::ObserverList<component_updater::ServiceObserver>::Unchecked
      observer_list_;
};

class FakeComponent {
 public:
  FakeComponent(std::string id, uint64_t total_bytes);

  component_updater::CrxUpdateItem CreateUpdateItem(
      update_client::ComponentState state,
      uint64_t downloaded_bytes);

  const std::string& id() { return id_; }
  uint64_t total_bytes() { return total_bytes_; }
  uint64_t downloaded_bytes() { return downloaded_bytes_; }

 private:
  std::string id_;
  uint64_t total_bytes_;
  uint64_t downloaded_bytes_ = 0;
};

}  // namespace optimization_guide
#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_COMPONENT_UPDATE_SERVICE_H_
