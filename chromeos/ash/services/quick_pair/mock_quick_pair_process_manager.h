// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_MOCK_QUICK_PAIR_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_MOCK_QUICK_PAIR_PROCESS_MANAGER_H_

#include <memory>

#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockQuickPairProcessManager : public QuickPairProcessManager {
 public:
  class MockProcessReference : public ProcessReference {
   public:
    MockProcessReference();
    MockProcessReference(const MockProcessReference&) = delete;
    MockProcessReference& operator=(const MockProcessReference&) = delete;
    ~MockProcessReference() override;

    MOCK_METHOD(const mojo::SharedRemote<mojom::FastPairDataParser>&,
                GetFastPairDataParser,
                (),
                (const, override));
  };

  MockQuickPairProcessManager();
  MockQuickPairProcessManager(const MockQuickPairProcessManager&) = delete;
  MockQuickPairProcessManager& operator=(const MockQuickPairProcessManager&) =
      delete;
  ~MockQuickPairProcessManager() override;

  MOCK_METHOD(std::unique_ptr<ProcessReference>,
              GetProcessReference,
              (ProcessStoppedCallback callback),
              (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_MOCK_QUICK_PAIR_PROCESS_MANAGER_H_
