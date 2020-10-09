// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_

#include <windows.applicationmodel.datatransfer.h>
#include <wrl/implements.h>
#include <vector>

#include "base/callback_forward.h"

namespace webshare {

// Provides an implementation of IDataTransferManager for use in GTests.
class FakeDataTransferManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager> {
 public:
  FakeDataTransferManager();
  FakeDataTransferManager(const FakeDataTransferManager&) = delete;
  FakeDataTransferManager& operator=(const FakeDataTransferManager&) = delete;
  ~FakeDataTransferManager() override;

  // ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager:
  IFACEMETHODIMP add_DataRequested(
      __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CDataRequestedEventArgs*
          event_handler,
      EventRegistrationToken* event_cookie) override;
  IFACEMETHODIMP
  remove_DataRequested(EventRegistrationToken event_cookie) override;
  IFACEMETHODIMP add_TargetApplicationChosen(
      __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CTargetApplicationChosenEventArgs*
          eventHandler,
      EventRegistrationToken* event_cookie) override;
  IFACEMETHODIMP
  remove_TargetApplicationChosen(EventRegistrationToken event_cookie) override;

  // Returns a callback that captures a reference to the current DataRequested
  // event handler and, when invoked, triggers that handler.
  //
  // If the registered handler changes after this method is called the callback
  // will still trigger the previous event handler, not a newly registered one.
  base::OnceClosure GetDataRequestedInvoker();

  bool HasDataRequestedListener();

 private:
  struct DataRequestedHandlerEntry {
    DataRequestedHandlerEntry();
    DataRequestedHandlerEntry(DataRequestedHandlerEntry const& other);
    ~DataRequestedHandlerEntry();

    Microsoft::WRL::ComPtr<
        __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CDataRequestedEventArgs>
        event_handler_;
    int64_t token_value_;
  };

  std::vector<DataRequestedHandlerEntry> data_requested_event_handlers_;
  int64_t latest_token_value_ = 0;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_
