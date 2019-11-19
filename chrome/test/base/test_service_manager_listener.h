// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_SERVICE_MANAGER_LISTENER_H_
#define CHROME_TEST_BASE_TEST_SERVICE_MANAGER_LISTENER_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace service_manager {
class Identity;
}

// This class lets us wait for services to be started and tracks how many times
// a service was started.
class TestServiceManagerListener
    : public service_manager::mojom::ServiceManagerListener {
 public:
  TestServiceManagerListener();
  ~TestServiceManagerListener() override;

  // Must be called once before the other public methods can be used.
  void Init();

  void WaitUntilServiceStarted(const std::string& service_name);
  uint32_t GetServiceStartCount(const std::string& service_name) const;

 private:
  // service_manager::mojom::ServiceManagerListener implementation:
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override;
  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override;
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override;
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override;
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override;
  void OnServiceStopped(const service_manager::Identity& identity) override;

  base::Closure on_service_event_loop_closure_;
  std::string service_name_;
  std::map<std::string, uint32_t> service_start_counters_;

  mojo::Receiver<service_manager::mojom::ServiceManagerListener> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceManagerListener);
};

#endif  // CHROME_TEST_BASE_TEST_SERVICE_MANAGER_LISTENER_H_
