// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

namespace {

class TestObserver : public SafetyHubService::Observer {
 public:
  void SetCallback(const base::RepeatingClosure& callback) {
    callback_ = callback;
  }

  void OnResultAvailable(const SafetyHubService::Result* result) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

namespace safety_hub_test_util {

void UpdateSafetyHubServiceAsync(SafetyHubService* service) {
  auto test_observer = std::make_shared<TestObserver>();
  service->AddObserver(test_observer.get());
  // We need to check if there is any update process currently active, and wait
  // until all have completed before running another update.
  while (service->IsUpdateRunning()) {
    base::RunLoop ongoing_update_loop;
    test_observer->SetCallback(ongoing_update_loop.QuitClosure());
    ongoing_update_loop.Run();
  }
  base::RunLoop loop;
  test_observer->SetCallback(loop.QuitClosure());
  service->UpdateAsync();
  loop.Run();
  service->RemoveObserver(test_observer.get());
}

}  // namespace safety_hub_test_util
