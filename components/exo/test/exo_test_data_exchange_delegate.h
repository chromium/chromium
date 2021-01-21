// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_

#include "components/exo/data_exchange_delegate.h"

class GURL;

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
  std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const override;
  std::string GetMimeTypeForUriList(ui::EndpointType target) const override;
  void SendFileInfo(ui::EndpointType target,
                    const std::vector<ui::FileInfo>& files,
                    SendDataCallback callback) const override;
  bool HasUrlsInPickle(const base::Pickle& pickle) const override;
  void SendPickle(ui::EndpointType target,
                  const base::Pickle& pickle,
                  SendDataCallback callback) override;

  void RunSendPickleCallback(std::vector<GURL> urls);

 private:
  SendDataCallback send_pickle_callback_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_DATA_EXCHANGE_DELEGATE_H_
