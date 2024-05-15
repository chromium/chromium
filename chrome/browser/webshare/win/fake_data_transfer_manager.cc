// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"

#include <wrl/module.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/vector.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::ApplicationModel::DataTransfer::DataPackage;
using ABI::Windows::ApplicationModel::DataTransfer::DataPackageOperation;
using ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager;
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
using ABI::Windows::ApplicationModel::DataTransfer::OperationCompletedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::
    TargetApplicationChosenEventArgs;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Foundation::Collections::IIterable;
using ABI::Windows::Foundation::Collections::IIterator;
using ABI::Windows::Foundation::Collections::IMap;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageItem;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using ABI::Windows::Storage::Streams::RandomAccessStreamReference;
using Microsoft::WRL::ActivationFactory;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRtClassicComMix;

namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

// Define template specializations for the types used.
template <>
struct __declspec(uuid("AF82EEF9-F786-475D-A3EB-929AEB6F0689"))
    IObservableVector<HSTRING> : IObservableVector_impl<HSTRING> {};

template <>
struct __declspec(uuid("1ED11184-03B9-4911-875C-9682969C732A"))
    VectorChangedEventHandler<HSTRING>
    : VectorChangedEventHandler_impl<HSTRING> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace webshare {
namespace {

class FakeDataPackagePropertySet final
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
  ~FakeDataPackagePropertySet() final {
    // Though it is technically legal for consuming code to hold on to the
    // FileTypes past the lifetime of the DataPackagePropertySet, there is
    // no good reason to do so, so any lingering references presumably point
    // to a coding error.
    if (file_types_)
      EXPECT_EQ(0u, file_types_.Reset());
  }

  // IDataPackagePropertySet
  IFACEMETHODIMP get_ApplicationListingUri(IUriRuntimeClass** value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ApplicationName(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Description(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FileTypes(IVector<HSTRING>** value) final {
    if (!file_types_)
      file_types_ = Make<base::win::Vector<HSTRING>>();
    auto hr = file_types_->QueryInterface(IID_PPV_ARGS(value));
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  IFACEMETHODIMP get_Thumbnail(IRandomAccessStreamReference** value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Title(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_ApplicationListingUri(IUriRuntimeClass* value) final {
    return S_OK;
  }
  IFACEMETHODIMP put_ApplicationName(HSTRING value) final { return S_OK; }
  IFACEMETHODIMP put_Description(HSTRING value) final { return S_OK; }
  IFACEMETHODIMP put_Thumbnail(IRandomAccessStreamReference* value) final {
    return S_OK;
  }
  IFACEMETHODIMP put_Title(HSTRING value) final {
    base::win::ScopedHString wrapped_value(value);
    data_requested_content_->title = wrapped_value.GetAsUTF8();
    return S_OK;
  }

  // IDataPackagePropertySet3
  IFACEMETHODIMP get_EnterpriseId(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_EnterpriseId(HSTRING value) final { return S_OK; }

 private:
  const raw_ref<FakeDataTransferManager::DataRequestedContent,
                DanglingUntriaged>
      data_requested_content_;
  ComPtr<base::win::Vector<HSTRING>> file_types_;
};

class FakeDataPackage final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataPackage,
                          IDataPackage2> {
 public:
  FakeDataPackage(
      FakeDataTransferManager::DataRequestedContent& data_requested_content)
      : data_requested_content_(data_requested_content) {}
  FakeDataPackage(const FakeDataPackage&) = delete;
  FakeDataPackage& operator=(const FakeDataPackage&) = delete;
  ~FakeDataPackage() final {
    // Though it is technically legal for consuming code to hold on to the
    // DataPackagePropertySet past the lifetime of the DataPackage, there is
    // no good reason to do so, so any lingering references presumably point
    // to a coding error.
    if (properties_)
      EXPECT_EQ(0u, properties_.Reset());
  }

  // IDataPackage
  IFACEMETHODIMP add_Destroyed(
      ITypedEventHandler<DataPackage*, IInspectable*>* handler,
      EventRegistrationToken* token) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP add_OperationCompleted(
      ITypedEventHandler<DataPackage*, OperationCompletedEventArgs*>* handler,
      EventRegistrationToken* token) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetView(IDataPackageView** result) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Properties(IDataPackagePropertySet** value) final {
    if (!properties_)
      properties_ = Make<FakeDataPackagePropertySet>(*data_requested_content_);
    auto hr = properties_->QueryInterface(IID_PPV_ARGS(value));
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  IFACEMETHODIMP get_RequestedOperation(DataPackageOperation* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ResourceMap(
      IMap<HSTRING, RandomAccessStreamReference*>** value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_RequestedOperation(DataPackageOperation value) final {
    return S_OK;
  }
  IFACEMETHODIMP remove_Destroyed(EventRegistrationToken token) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP remove_OperationCompleted(EventRegistrationToken token) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetBitmap(IRandomAccessStreamReference* value) final {
    return S_OK;
  }
  IFACEMETHODIMP SetData(HSTRING formatId, IInspectable* value) final {
    return S_OK;
  }
  IFACEMETHODIMP SetDataProvider(HSTRING formatId,
                                 IDataProviderHandler* delayRenderer) final {
    return S_OK;
  }
  IFACEMETHODIMP SetHtmlFormat(HSTRING value) final { return S_OK; }
  IFACEMETHODIMP SetRtf(HSTRING value) final { return S_OK; }
  IFACEMETHODIMP SetText(HSTRING value) final {
    base::win::ScopedHString wrapped_value(value);
    data_requested_content_->text = wrapped_value.GetAsUTF8();
    return S_OK;
  }
  IFACEMETHODIMP SetStorageItems(IIterable<IStorageItem*>* value,
                                 boolean readOnly) final {
    EXPECT_TRUE(readOnly);
    return SetStorageItemsReadOnly(value);
  }
  IFACEMETHODIMP SetStorageItemsReadOnly(
      IIterable<IStorageItem*>* value) final {
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
      data_requested_content_->files.push_back(std::move(file));

      hr = iterator->MoveNext(&has_current);
      if (FAILED(hr))
        return hr;
    }
    return S_OK;
  }
  IFACEMETHODIMP SetUri(IUriRuntimeClass* value) final { return S_OK; }

  // IDataPackage2
  IFACEMETHODIMP SetApplicationLink(IUriRuntimeClass* value) final {
    return S_OK;
  }
  IFACEMETHODIMP SetWebLink(IUriRuntimeClass* value) final {
    HSTRING raw_uri;
    value->get_RawUri(&raw_uri);
    base::win::ScopedHString wrapped_value(raw_uri);
    data_requested_content_->uri = wrapped_value.GetAsUTF8();
    return S_OK;
  }

 private:
  const raw_ref<FakeDataTransferManager::DataRequestedContent,
                DanglingUntriaged>
      data_requested_content_;
  ComPtr<IDataPackagePropertySet> properties_;
};

class FakeDataRequest final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IDataRequest> {
 public:
  struct FakeDataRequestDeferral final
      : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                            IDataRequestDeferral> {
   public:
    explicit FakeDataRequestDeferral(FakeDataRequest* data_request)
        : data_request_(data_request) {}
    FakeDataRequestDeferral(const FakeDataRequestDeferral&) = delete;
    FakeDataRequestDeferral& operator=(const FakeDataRequestDeferral&) = delete;

    // IDataRequestDeferral
    IFACEMETHODIMP Complete() final {
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
  ~FakeDataRequest() final = default;

  // IDataRequest
  IFACEMETHODIMP FailWithDisplayText(HSTRING value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Data(IDataPackage** value) final {
    if (!data_package_)
      data_package_ = Make<FakeDataPackage>(data_requested_content_);
    auto hr = data_package_->QueryInterface(IID_PPV_ARGS(value));
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  IFACEMETHODIMP
  get_Deadline(DateTime* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetDeferral(IDataRequestDeferral** value) final {
    if (!data_request_deferral_)
      data_request_deferral_ = Make<FakeDataRequestDeferral>(this);
    auto hr = data_request_deferral_->QueryInterface(IID_PPV_ARGS(value));
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  IFACEMETHODIMP put_Data(IDataPackage* value) final {
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

class FakeDataRequestedEventArgs final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IDataRequestedEventArgs> {
 public:
  FakeDataRequestedEventArgs(FakeDataTransferManager::PostDataRequestedCallback
                                 post_data_requested_callback)
      : post_data_requested_callback_(post_data_requested_callback) {}
  FakeDataRequestedEventArgs(const FakeDataRequestedEventArgs&) = delete;
  FakeDataRequestedEventArgs& operator=(const FakeDataRequestedEventArgs&) =
      delete;
  ~FakeDataRequestedEventArgs() final = default;

  // IDataRequestedEventArgs
  IFACEMETHODIMP get_Request(IDataRequest** value) final {
    if (!data_request_)
      data_request_ = Make<FakeDataRequest>(post_data_requested_callback_);
    auto hr = data_request_->QueryInterface(IID_PPV_ARGS(value));
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
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
    ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>*
        event_handler,
    EventRegistrationToken* event_cookie) {
  DataRequestedHandlerEntry entry;
  entry.event_handler = event_handler;
  entry.token_value = ++latest_token_value_;
  data_requested_event_handlers_.push_back(std::move(entry));
  event_cookie->value = latest_token_value_;
  return S_OK;
}

IFACEMETHODIMP
FakeDataTransferManager::remove_DataRequested(
    EventRegistrationToken event_cookie) {
  auto it = data_requested_event_handlers_.begin();
  while (it != data_requested_event_handlers_.end()) {
    if (it->token_value == event_cookie.value) {
      data_requested_event_handlers_.erase(it);
      return S_OK;
    }
    it++;
  }
  ADD_FAILURE() << "remove_DataRequested called for untracked token";
  return E_FAIL;
}

IFACEMETHODIMP FakeDataTransferManager::add_TargetApplicationChosen(
    ITypedEventHandler<DataTransferManager*, TargetApplicationChosenEventArgs*>*
        eventHandler,
    EventRegistrationToken* event_cookie) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP
FakeDataTransferManager::remove_TargetApplicationChosen(
    EventRegistrationToken event_cookie) {
  NOTREACHED_IN_MIGRATION();
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
  auto handler = data_requested_event_handlers_.back().event_handler;
  ComPtr<FakeDataTransferManager> self = this;
  return base::BindOnce(
      [](ComPtr<FakeDataTransferManager> self,
         ComPtr<ITypedEventHandler<DataTransferManager*,
                                   DataRequestedEventArgs*>> handler) {
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
  // Check that the event handler has not been over-freed.
  //
  // An explicit call to Reset() will cause an Access Violation exception if the
  // reference count is already at 0. Though the underling ComPtr code does a
  // similar check on destruction of the ComPtr, it does not throw an exception
  // in that case, so we have to call Reset() to have the failure exposed to us.
  //
  // We cannot assume that this particular ComPtr is the last reference to the
  // event handler, so do not check to see if the value returned by Reset() is
  // 0.
  event_handler.Reset();
}

}  // namespace webshare
