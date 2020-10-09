// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"

#include <wrl/module.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {

using ABI::Windows::ApplicationModel::DataTransfer::DataPackageOperation;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet3;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackageView;
using ABI::Windows::ApplicationModel::DataTransfer::IDataProviderHandler;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequest;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestDeferral;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using Microsoft::WRL::ActivationFactory;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRtClassicComMix;

using DataRequestedEventHandler =
    __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CDataRequestedEventArgs;
using DestroyedEventHandler =
    __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataPackage_IInspectable;
using OperationCompletedEventHandler =
    __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataPackage_Windows__CApplicationModel__CDataTransfer__COperationCompletedEventArgs;
using ResourceMap =
    __FIMap_2_HSTRING_Windows__CStorage__CStreams__CRandomAccessStreamReference;
using StorageItems = __FIIterable_1_Windows__CStorage__CIStorageItem;
using TargetApplicationChosenEventHandler =
    __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CTargetApplicationChosenEventArgs;

namespace {

class FakeDataPackagePropertySet
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataPackagePropertySet,
                          IDataPackagePropertySet3> {
 public:
  FakeDataPackagePropertySet() = default;
  FakeDataPackagePropertySet(const FakeDataPackagePropertySet&) = delete;
  FakeDataPackagePropertySet& operator=(const FakeDataPackagePropertySet&) =
      delete;

  // IDataPackagePropertySet
  IFACEMETHODIMP get_ApplicationListingUri(IUriRuntimeClass** value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ApplicationName(HSTRING* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Description(HSTRING* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FileTypes(__FIVector_1_HSTRING** value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Thumbnail(IRandomAccessStreamReference** value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Title(HSTRING* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_ApplicationListingUri(IUriRuntimeClass* value) override {
    return S_OK;
  }
  IFACEMETHODIMP put_ApplicationName(HSTRING value) override { return S_OK; }
  IFACEMETHODIMP put_Description(HSTRING value) override { return S_OK; }
  IFACEMETHODIMP put_Thumbnail(IRandomAccessStreamReference* value) override {
    return S_OK;
  }
  IFACEMETHODIMP put_Title(HSTRING value) override { return S_OK; }

  // IDataPackagePropertySet3
  IFACEMETHODIMP get_EnterpriseId(HSTRING* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_EnterpriseId(HSTRING value) override { return S_OK; }
};

class FakeDataPackage
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataPackage,
                          IDataPackage2> {
 public:
  FakeDataPackage() = default;
  FakeDataPackage(const FakeDataPackage&) = delete;
  FakeDataPackage& operator=(const FakeDataPackage&) = delete;
  ~FakeDataPackage() override {
    // Though it is technically legal for consuming code to hold on to the
    // DataPackagePropertySet past the lifetime of the DataPackage, there is
    // no good reason to do so, so any lingering references presumably point
    // to a coding error.
    if (properties_)
      EXPECT_EQ(0u, properties_.Reset());
  }

  // IDataPackage
  IFACEMETHODIMP add_Destroyed(DestroyedEventHandler* handler,
                               EventRegistrationToken* token) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP add_OperationCompleted(
      OperationCompletedEventHandler* handler,
      EventRegistrationToken* token) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetView(IDataPackageView** result) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Properties(IDataPackagePropertySet** value) override {
    if (!properties_)
      properties_ = Make<FakeDataPackagePropertySet>();
    *value = properties_.Get();
    properties_->AddRef();
    return S_OK;
  }
  IFACEMETHODIMP get_RequestedOperation(DataPackageOperation* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ResourceMap(ResourceMap** value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_RequestedOperation(DataPackageOperation value) override {
    return S_OK;
  }
  IFACEMETHODIMP remove_Destroyed(EventRegistrationToken token) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP remove_OperationCompleted(
      EventRegistrationToken token) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetBitmap(IRandomAccessStreamReference* value) override {
    return S_OK;
  }
  IFACEMETHODIMP SetData(HSTRING formatId, IInspectable* value) override {
    return S_OK;
  }
  IFACEMETHODIMP SetDataProvider(HSTRING formatId,
                                 IDataProviderHandler* delayRenderer) override {
    return S_OK;
  }
  IFACEMETHODIMP SetHtmlFormat(HSTRING value) override { return S_OK; }
  IFACEMETHODIMP SetRtf(HSTRING value) override { return S_OK; }
  IFACEMETHODIMP SetText(HSTRING value) override { return S_OK; }
  IFACEMETHODIMP SetStorageItems(StorageItems* value,
                                 boolean readOnly) override {
    return S_OK;
  }
  IFACEMETHODIMP SetStorageItemsReadOnly(StorageItems* value) override {
    return S_OK;
  }
  IFACEMETHODIMP SetUri(IUriRuntimeClass* value) override { return S_OK; }

  // IDataPackage2
  IFACEMETHODIMP SetApplicationLink(IUriRuntimeClass* value) override {
    return S_OK;
  }
  IFACEMETHODIMP SetWebLink(IUriRuntimeClass* value) override { return S_OK; }

 private:
  ComPtr<IDataPackagePropertySet> properties_;
};

class FakeDataRequest
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IDataRequest> {
 public:
  FakeDataRequest() = default;
  FakeDataRequest(const FakeDataRequest&) = delete;
  FakeDataRequest& operator=(const FakeDataRequest&) = delete;
  ~FakeDataRequest() override {
    // Though it is technically legal for consuming code to hold on to the
    // DataPackage past the lifetime of the DataRequest, there is no good
    // reason to do so, so any lingering references presumably point to a
    // coding error.
    if (data_package_)
      EXPECT_EQ(0u, data_package_.Reset());
  }

  // IDataRequest
  IFACEMETHODIMP FailWithDisplayText(HSTRING value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Data(IDataPackage** value) override {
    if (!data_package_)
      data_package_ = Make<FakeDataPackage>();
    *value = data_package_.Get();
    data_package_->AddRef();
    return S_OK;
  }
  IFACEMETHODIMP
  get_Deadline(DateTime* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetDeferral(IDataRequestDeferral** value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_Data(IDataPackage* value) override {
    data_package_ = value;
    return S_OK;
  }

 private:
  ComPtr<IDataPackage> data_package_;
};

class FakeDataRequestedEventArgs
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataRequestedEventArgs> {
 public:
  FakeDataRequestedEventArgs() = default;
  FakeDataRequestedEventArgs(const FakeDataRequestedEventArgs&) = delete;
  FakeDataRequestedEventArgs& operator=(const FakeDataRequestedEventArgs&) =
      delete;
  ~FakeDataRequestedEventArgs() override {
    // Though it is technically legal for consuming code to hold on to the
    // DataRequest past the lifetime of the DataRequestedEventArgs, there is
    // no good reason to do so, so any lingering references presumably point
    // to a coding error.
    if (data_request_)
      EXPECT_EQ(0u, data_request_.Reset());
  }

  // IDataRequestedEventArgs
  IFACEMETHODIMP get_Request(IDataRequest** value) override {
    if (!data_request_)
      data_request_ = Make<FakeDataRequest>();
    *value = data_request_.Get();
    data_request_->AddRef();
    return S_OK;
  }

 private:
  ComPtr<IDataRequest> data_request_;
};

}  // namespace

FakeDataTransferManager::FakeDataTransferManager() = default;
FakeDataTransferManager::~FakeDataTransferManager() = default;

IFACEMETHODIMP
FakeDataTransferManager::add_DataRequested(
    DataRequestedEventHandler* event_handler,
    EventRegistrationToken* event_cookie) {
  DataRequestedHandlerEntry entry;
  entry.event_handler_ = event_handler;
  entry.token_value_ = ++latest_token_value_;
  data_requested_event_handlers_.push_back(std::move(entry));
  event_cookie->value = latest_token_value_;
  return S_OK;
}

IFACEMETHODIMP
FakeDataTransferManager::remove_DataRequested(
    EventRegistrationToken event_cookie) {
  auto it = data_requested_event_handlers_.begin();
  while (it != data_requested_event_handlers_.end()) {
    if (it->token_value_ == event_cookie.value) {
      data_requested_event_handlers_.erase(it);
      return S_OK;
    }
    it++;
  }
  ADD_FAILURE() << "remove_DataRequested called for untracked token";
  return E_FAIL;
}

IFACEMETHODIMP FakeDataTransferManager::add_TargetApplicationChosen(
    TargetApplicationChosenEventHandler* eventHandler,
    EventRegistrationToken* event_cookie) {
  NOTREACHED();
  return E_NOTIMPL;
}

IFACEMETHODIMP
FakeDataTransferManager::remove_TargetApplicationChosen(
    EventRegistrationToken event_cookie) {
  NOTREACHED();
  return E_NOTIMPL;
}

base::OnceClosure FakeDataTransferManager::GetDataRequestedInvoker() {
  if (data_requested_event_handlers_.empty()) {
    ADD_FAILURE()
        << "GetDataRequestedInvoker called with no event handler registered";
    return base::DoNothing();
  }

  // Though multiple handlers may be registered for this event, only the
  // latest is invoked by the OS and then the event is considered handled.
  auto handler = data_requested_event_handlers_.back().event_handler_;
  ComPtr<IDataTransferManager> self = this;
  return base::BindOnce(
      [](ComPtr<IDataTransferManager> self,
         ComPtr<DataRequestedEventHandler> handler) {
        ComPtr<IDataRequestedEventArgs> event_args =
            Make<FakeDataRequestedEventArgs>();
        handler->Invoke(self.Get(), event_args.Get());
      },
      self, handler);
}

bool FakeDataTransferManager::HasDataRequestedListener() {
  return !data_requested_event_handlers_.empty();
}

FakeDataTransferManager::DataRequestedHandlerEntry::
    DataRequestedHandlerEntry() = default;
FakeDataTransferManager::DataRequestedHandlerEntry::DataRequestedHandlerEntry(
    DataRequestedHandlerEntry const& other) = default;

FakeDataTransferManager::DataRequestedHandlerEntry::
    ~DataRequestedHandlerEntry() {
  // Check that the ComPtr<DataRequestedEventHandler> has not been over-freed.
  //
  // An explicit call to Reset() will cause an Access Violation exception if the
  // reference count is already at 0. Though the underling ComPtr code does a
  // similar check on destruction of the ComPtr, it does not throw an exception
  // in that case, so we have to call Reset() to have the failure exposed to us.
  //
  // We cannot assume that this particular ComPtr is the last reference to the
  // DataRequestedEventHandler, so do not check to see if the value returned by
  // Reset() is 0.
  event_handler_.Reset();
}

}  // namespace webshare
