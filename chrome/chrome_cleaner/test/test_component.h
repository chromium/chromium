// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_COMPONENT_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_COMPONENT_H_

#include <vector>

#include "chrome/chrome_cleaner/components/component_api.h"

namespace chrome_cleaner {

class TestComponent : public ComponentAPI {
 public:
  struct Calls {
    Calls();
    ~Calls();

    bool pre_scan = false;
    bool post_scan = false;
    bool pre_cleanup = false;
    bool post_cleanup = false;
    bool post_validation = false;
    bool on_close = false;
    bool destroyed = false;
    std::vector<UwSId> post_scan_found_pups;
    ResultCode result_code = RESULT_CODE_INVALID;
  };

  explicit TestComponent(Calls* calls) : calls_(calls) {}
  ~TestComponent() override;

  // ComponentAPI.
  void PreScan() override;
  void PostScan(const std::vector<UwSId>& found_pups) override;
  void PreCleanup() override;
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) override;
  void PostValidation(ResultCode result_code) override;
  void OnClose(ResultCode result_code) override;

 private:
  Calls* calls_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_COMPONENT_H_
