// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_

#include "components/exo/data_exchange_delegate.h"

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace exo {

class TestDataExchangeDelegate : public DataExchangeDelegate {
 public:
  TestDataExchangeDelegate();
  TestDataExchangeDelegate(const TestDataExchangeDelegate&) = delete;
  TestDataExchangeDelegate& operator=(const TestDataExchangeDelegate&) = delete;
  ~TestDataExchangeDelegate() override;

  // DataExchangeDelegate:
  ui::EndpointType GetDataTransferEndpointType(
      aura::Window* window) const override;
  std::string GetMimeTypeForUriList(ui::EndpointType target) const override;
  bool HasUrlsInPickle(const base::Pickle& pickle) const override;
  std::vector<ui::FileInfo> ParseFileSystemSources(
      const ui::DataTransferEndpoint* source,
      const base::Pickle& pickle) const override;

  void set_endpoint_type(ui::EndpointType endpoint_type) {
    endpoint_type_ = endpoint_type;
  }

 private:
  ui::EndpointType endpoint_type_ = ui::EndpointType::kUnknownVm;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_
