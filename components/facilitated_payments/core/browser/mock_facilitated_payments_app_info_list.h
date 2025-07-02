// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_APP_INFO_LIST_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_APP_INFO_LIST_H_

#include <cstddef>

#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {

class MockFacilitatedPaymentsAppInfoList
    : public FacilitatedPaymentsAppInfoList {
 public:
  MockFacilitatedPaymentsAppInfoList();
  ~MockFacilitatedPaymentsAppInfoList() override;

  MOCK_METHOD(size_t, Size, (), (const, override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_APP_INFO_LIST_H_
