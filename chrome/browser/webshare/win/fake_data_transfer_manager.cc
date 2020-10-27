// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"

#include <wrl/module.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
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
using ABI::Windows::Foundation::Collections::IIterator;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageItem;
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
  FakeDataPackagePropertySet(
      FakeDataTransferManager::DataRequestedContent& data_requested_content)
      : data_requested_content_(data_requested_content) {}
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
  IFACEMETHODIMP put_Title(HSTRING value) override {
    base::win::ScopedHString wrapped_value(value);
    data_requested_content_.title = wrapped_value.GetAsUTF8();
    return S_OK;
  }

  // IDataPackagePropertySet3
  IFACEMETHODIMP get_EnterpriseId(HSTRING* value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_EnterpriseId(HSTRING value) override { return S_OK; }

 private:
  FakeDataTransferManager::DataRequestedContent& data_requested_content_;
};

class FakeDataPackage
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataPackage,
                          IDataPackage2> {
 public:
  FakeDataPackage(
      FakeDataTransferManager::DataRequestedContent& data_requested_content)
      : data_requested_content_(data_requested_content) {}
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
      properties_ = Make<FakeDataPackagePropertySet>(data_requested_content_);
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
  IFACEMETHODIMP SetText(HSTRING value) override {
    base::win::ScopedHString wrapped_value(value);
    data_requested_content_.text = wrapped_value.GetAsUTF8();
    return S_OK;
  }
  IFACEMETHODIMP SetStorageItems(StorageItems* value,
                                 boolean readOnly) override {
    EXPECT_TRUE(readOnly);
    return SetStorageItemsReadOnly(value);
  }
  IFACEMETHODIMP SetStorageItemsReadOnly(StorageItems* value) override {
    ComPtr<IIterator<IStorageItem*>> iterator;
    HRESULT hr = value->First(&iterator);
    if (FAILED(hr))
      return hr;
    boolean has_current;
    hr = iterator->get_HasCurrent(&has_current);
    if (FAILED(hr))
      return hr;
    while (has_current == TRUE) {
      ComPtr<IStorageItem> storage_item;
      hr = iterator->get_Current(&storage_item);
      if (FAILED(hr))
        return hr;

      HSTRING name;
      hr = storage_item->get_Name(&name);
      base::win::ScopedHString wrapped_name(name);
      if (FAILED(hr))
        return hr;

      ComPtr<IStorageFile> storage_file;
      hr = storage_item.As(&storage_file);
      if (FAILED(hr))
        return hr;

      FakeDataTransferManager::DataRequestedFile file;
      file.name = wrapped_name.GetAsUTF8();
      file.file = storage_file;
      data_requested_content_.files.push_back(std::move(file));

      hr = iterator->MoveNext(&has_current);
      if (FAILED(hr))
        return hr;
    }
    return S_OK;
  }
  IFACEMETHODIMP SetUri(IUriRuntimeClass* value) override {
    HSTRING raw_uri;
    value->get_RawUri(&raw_uri);
    base::win::ScopedHString wrapped_value(raw_uri);
    data_requested_content_.uri = wrapped_value.GetAsUTF8();
    return S_OK;
  }

  // IDataPackage2
  IFACEMETHODIMP SetApplicationLink(IUriRuntimeClass* value) override {
    return S_OK;
  }
  IFACEMETHODIMP SetWebLink(IUriRuntimeClass* value) override { return S_OK; }

 private:
  FakeDataTransferManager::DataRequestedContent& data_requested_content_;
  ComPtr<IDataPackagePropertySet> properties_;
};

class FakeDataRequest
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IDataRequest> {
 public:
  struct FakeDataRequestDeferral
      : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                            IDataRequestDeferral> {
   public:
    explicit FakeDataRequestDeferral(FakeDataRequest* data_request)
        : data_request_(data_request) {}
    FakeDataRequestDeferral(const FakeDataRequestDeferral&) = delete;
    FakeDataRequestDeferral& operator=(const FakeDataRequestDeferral&) = delete;

    // IDataRequestDeferral
    IFACEMETHODIMP Complete() override {
      data_request_->RunPostDataRequestedCallbackImpl();
      return S_OK;
    }

   private:
    ComPtr<FakeDataRequest> data_request_;
  };

  FakeDataRequest(FakeDataTransferManager::PostDataRequestedCallback
                      post_data_requested_callback)
      : post_data_requested_callback_(post_data_requested_callback) {}
  FakeDataRequest(const FakeDataRequest&) = delete;
  FakeDataRequest& operator=(const FakeDataRequest&) = delete;
  ~FakeDataRequest() override = default;

  // IDataRequest
  IFACEMETHODIMP FailWithDisplayText(HSTRING value) override {
    NOTREACHED();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Data(IDataPackage** value) override {
    if (!data_package_)
      data_package_ = Make<FakeDataPackage>(data_requested_content_);
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
    if (!data_request_deferral_)
      data_request_deferral_ = Make<FakeDataRequestDeferral>(this);
    *value = data_request_deferral_.Get();
    data_request_deferral_->AddRef();
    return S_OK;
  }
  IFACEMETHODIMP put_Data(IDataPackage* value) override {
    data_package_ = value;
    return S_OK;
  }

  void RunPostDataRequestedCallback() {
    // If there is not a deferral trigger the callback right away, otherwise it
    // will be triggered when the deferral is complete
    if (!data_request_deferral_)
      RunPostDataRequestedCallbackImpl();
  }

 private:
  void RunPostDataRequestedCallbackImpl() {
    post_data_requested_callback_.Run(data_requested_content_);
  }

  ComPtr<IDataPackage> data_package_;
  ComPtr<FakeDataRequestDeferral> data_request_deferral_;
  FakeDataTransferManager::DataRequestedContent data_requested_content_;
  FakeDataTransferManager::PostDataRequestedCallback
      post_data_requested_callback_;
};

class FakeDataRequestedEventArgs
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataRequestedEventArgs> {
 public:
  FakeDataRequestedEventArgs(FakeDataTransferManager::PostDataRequestedCallback
                                 post_data_requested_callback)
      : post_data_requested_callback_(post_data_requested_callback) {}
  FakeDataRequestedEventArgs(const FakeDataRequestedEventArgs&) = delete;
  FakeDataRequestedEventArgs& operator=(const FakeDataRequestedEventArgs&) =
      delete;
  ~FakeDataRequestedEventArgs() override = default;

  // IDataRequestedEventArgs
  IFACEMETHODIMP get_Request(IDataRequest** value) override {
    if (!data_request_)
      data_request_ = Make<FakeDataRequest>(post_data_requested_callback_);
    *value = data_request_.Get();
    data_request_->AddRef();
    return S_OK;
  }

  void RunPostDataRequestedCallback() {
    if (data_request_)
      data_request_->RunPostDataRequestedCallback();
  }

 private:
  ComPtr<FakeDataRequest> data_request_;
  FakeDataTransferManager::PostDataRequestedCallback
      post_data_requested_callback_;
};

}  // namespace

// static
bool FakeDataTransferManager::IsSupportedEnvironment() {
  if (base::win::ResolveCoreWinRTDelayload() &&
      base::win::ScopedHString::ResolveCoreWinRTStringDelayload())
    return true;
  EXPECT_LT(base::win::GetVersion(), base::win::Version::WIN8);
  return false;
}

FakeDataTransferManager::FakeDataTransferManager() {
  post_data_requested_callback_ = base::DoNothing();
}
FakeDataTransferManager::~FakeDataTransferManager() = default;

FakeDataTransferManager::DataRequestedFile::DataRequestedFile() = default;
FakeDataTransferManager::DataRequestedFile::DataRequestedFile(
    FakeDataTransferManager::DataRequestedFile&&) = default;
FakeDataTransferManager::DataRequestedFile::~DataRequestedFile() = default;

FakeDataTransferManager::DataRequestedContent::DataRequestedContent() = default;
FakeDataTransferManager::DataRequestedContent::~DataRequestedContent() =
    default;

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
  ComPtr<FakeDataTransferManager> self = this;
  return base::BindOnce(
      [](ComPtr<FakeDataTransferManager> self,
         ComPtr<DataRequestedEventHandler> handler) {
        auto event_args = Make<FakeDataRequestedEventArgs>(
            self->post_data_requested_callback_);
        handler->Invoke(self.Get(), event_args.Get());
        event_args->RunPostDataRequestedCallback();
      },
      self, handler);
}

bool FakeDataTransferManager::HasDataRequestedListener() {
  return !data_requested_event_handlers_.empty();
}

void FakeDataTransferManager::SetPostDataRequestedCallback(
    PostDataRequestedCallback post_data_requested_callback) {
  post_data_requested_callback_ = std::move(post_data_requested_callback);
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
