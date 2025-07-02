// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_APP_INFO_LIST_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_APP_INFO_LIST_H_

namespace payments::facilitated {

// Interface for facilitated payment app information.
class FacilitatedPaymentsAppInfoList {
 public:
  virtual ~FacilitatedPaymentsAppInfoList() = default;
  virtual size_t Size() const = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_APP_INFO_LIST_H_
