// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RLZ_MOCK_RLZ_TRACKER_DELEGATE_H_
#define COMPONENTS_RLZ_MOCK_RLZ_TRACKER_DELEGATE_H_

#include "components/rlz/rlz_tracker_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace rlz {

class MockRLZTrackerDelegate : public RLZTrackerDelegate {
 public:
  MockRLZTrackerDelegate();
  MockRLZTrackerDelegate(const MockRLZTrackerDelegate&) = delete;
  MockRLZTrackerDelegate& operator=(const MockRLZTrackerDelegate&) = delete;
  ~MockRLZTrackerDelegate() override;

  MOCK_METHOD(void, Cleanup, (), (override));
  MOCK_METHOD(bool, IsOnUIThread, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(bool, GetBrand, (std::string * brand), (override));
  MOCK_METHOD(bool, IsBrandOrganic, (const std::string& brand), (override));
  MOCK_METHOD(bool, GetReactivationBrand, (std::string * brand), (override));
  MOCK_METHOD(bool, ShouldEnableZeroDelayForTesting, (), (override));
  MOCK_METHOD(bool, GetLanguage, (std::u16string * language), (override));
  MOCK_METHOD(bool, GetReferral, (std::u16string * referral), (override));
  MOCK_METHOD(bool, ClearReferral, (), (override));
  MOCK_METHOD(void,
              SetOmniboxSearchCallback,
              (base::OnceClosure callback),
              (override));
  MOCK_METHOD(void,
              SetHomepageSearchCallback,
              (base::OnceClosure callback),
              (override));
  MOCK_METHOD(void, RunHomepageSearchCallback, (), (override));
  MOCK_METHOD(bool, ShouldUpdateExistingAccessPointRlz, (), (override));
};

}  // namespace rlz

#endif  // COMPONENTS_RLZ_MOCK_RLZ_TRACKER_DELEGATE_H_
