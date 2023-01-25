// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_

#include <windows.applicationmodel.datatransfer.h>
#include <wrl/implements.h>
#include <vector>

#include "base/functional/callback.h"

namespace webshare {

// Provides an implementation of IDataTransferManager for use in GTests.
class __declspec(uuid("53CA4C00-6F19-40C1-A740-F66510E2DB40"))
    FakeDataTransferManager final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager> {
 public:
  // Represents a file surfaced to a DataRequested event
  struct DataRequestedFile {
    DataRequestedFile();
    DataRequestedFile(const DataRequestedFile&) = delete;
    DataRequestedFile& operator=(const DataRequestedFile&) = delete;
    DataRequestedFile(DataRequestedFile&&);
    ~DataRequestedFile();

    std::string name;
    Microsoft::WRL::ComPtr<ABI::Windows::Storage::IStorageFile> file;
  };

  // Represents the content surfaced to a DataRequested event
  struct DataRequestedContent {
    DataRequestedContent();
    DataRequestedContent(const DataRequestedContent&) = delete;
    DataRequestedContent& operator=(const DataRequestedContent&) = delete;
    ~DataRequestedContent();

    std::string text;
    std::string title;
    std::string uri;
    std::vector<DataRequestedFile> files;
  };

  using PostDataRequestedCallback =
      base::RepeatingCallback<void(const DataRequestedContent&)>;

  FakeDataTransferManager();
  FakeDataTransferManager(const FakeDataTransferManager&) = delete;
  FakeDataTransferManager& operator=(const FakeDataTransferManager&) = delete;
  ~FakeDataTransferManager() final;

  // ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager:
  IFACEMETHODIMP add_DataRequested(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager*,
          ABI::Windows::ApplicationModel::DataTransfer::
              DataRequestedEventArgs*>* event_handler,
      EventRegistrationToken* event_cookie) final;
  IFACEMETHODIMP
  remove_DataRequested(EventRegistrationToken event_cookie) final;
  IFACEMETHODIMP add_TargetApplicationChosen(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager*,
          ABI::Windows::ApplicationModel::DataTransfer::
              TargetApplicationChosenEventArgs*>* eventHandler,
      EventRegistrationToken* event_cookie) final;
  IFACEMETHODIMP
  remove_TargetApplicationChosen(EventRegistrationToken event_cookie) final;

  // Returns a callback that captures a reference to the current DataRequested
  // event handler and, when invoked, triggers that handler.
  //
  // If the registered handler changes after this method is called the callback
  // will still trigger the previous event handler, not a newly registered one.
  base::OnceClosure GetDataRequestedInvoker();

  bool HasDataRequestedListener();

  // Sets a callback that will be invoked after any DataRequested event is
  // triggered and passed the content supplied by the DataRequested handler
  void SetPostDataRequestedCallback(
      PostDataRequestedCallback post_data_requested_callback);

 private:
  struct DataRequestedHandlerEntry {
    DataRequestedHandlerEntry();
    DataRequestedHandlerEntry(DataRequestedHandlerEntry const& other);
    ~DataRequestedHandlerEntry();

    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager*,
        ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs*>>
        event_handler;
    int64_t token_value;
  };

  std::vector<DataRequestedHandlerEntry> data_requested_event_handlers_;
  int64_t latest_token_value_ = 0;
  PostDataRequestedCallback post_data_requested_callback_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_TRANSFER_MANAGER_H_
