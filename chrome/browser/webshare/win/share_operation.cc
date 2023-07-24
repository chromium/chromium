// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/share_operation.h"

#include <shlobj.h>
#include <windows.applicationmodel.datatransfer.h>
#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <wininet.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "base/win/vector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/win/show_share_ui_for_window_operation.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_writer_delegate.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "ui/base/win/internal_constants.h"
#include "ui/views/win/hwnd_util.h"
#include "url/gurl.h"

using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequest;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestDeferral;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Foundation::IUriRuntimeClassFactory;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageFileStatics;
using ABI::Windows::Storage::IStorageItem;
using ABI::Windows::Storage::IStreamedFileDataRequestedHandler;
using ABI::Windows::Storage::StorageFile;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IDataWriterFactory;
using ABI::Windows::Storage::Streams::IOutputStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace ABI::Windows::Foundation::Collections {

// Define template specializations for the types used. These uuids were randomly
// generated.
template <>
struct __declspec(uuid("CBE31E85-DEC8-4227-987F-9C63D6AA1A2E"))
    IObservableVector<IStorageItem*> : IObservableVector_impl<IStorageItem*> {};

template <>
struct __declspec(uuid("30BE4864-5EE5-4111-916E-15126649F3C9"))
    VectorChangedEventHandler<IStorageItem*>
    : VectorChangedEventHandler_impl<IStorageItem*> {};

}  // namespace ABI::Windows::Foundation::Collections

namespace webshare {
namespace {

uint64_t g_max_file_bytes = kMaxSharedFileBytes;
decltype(
    &base::win::RoGetActivationFactory) g_ro_get_activation_factory_function =
    &base::win::RoGetActivationFactory;

template <typename InterfaceType, wchar_t const* runtime_class_id>
HRESULT GetActivationFactory(InterfaceType** factory) {
  auto class_id_hstring = base::win::ScopedHString::Create(runtime_class_id);
  if (!class_id_hstring.is_valid())
    return E_FAIL;

  return g_ro_get_activation_factory_function(class_id_hstring.get(),
                                              IID_PPV_ARGS(factory));
}

// Implements FileStreamWriter for an IDataWriter.
class DataWriterFileStreamWriter final : public storage::FileStreamWriter {
 public:
  explicit DataWriterFileStreamWriter(
      ComPtr<IDataWriter> data_writer,
      scoped_refptr<base::RefCountedData<uint64_t>> file_bytes_shared)
      : data_writer_(data_writer), file_bytes_shared_(file_bytes_shared) {}

  int Cancel(net::CompletionOnceCallback callback) final {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // If there is no async operation in progress, Cancel() should
    // return net::ERR_UNEXPECTED per file_stream_header.h
    if (!flush_operation_ && !write_operation_)
      return net::ERR_UNEXPECTED;

    if (flush_operation_) {
      flush_callback_.Reset();
      ComPtr<IAsyncInfo> async_info;
      auto hr = flush_operation_.As(&async_info);
      if (FAILED(hr))
        return net::ERR_UNEXPECTED;

      hr = async_info->Cancel();
      if (FAILED(hr))
        return net::ERR_UNEXPECTED;

      flush_operation_.Reset();
    }

    if (write_operation_) {
      write_callback_.Reset();
      ComPtr<IAsyncInfo> async_info;
      auto hr = write_operation_.As(&async_info);
      if (FAILED(hr))
        return net::ERR_UNEXPECTED;

      hr = async_info->Cancel();
      if (FAILED(hr))
        return net::ERR_UNEXPECTED;

      write_operation_.Reset();
    }
    return net::OK;
  }

  int Flush(storage::FlushMode /*flush_mode*/,
            net::CompletionOnceCallback callback) final {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(flush_callback_.is_null());
    DCHECK_EQ(flush_operation_, nullptr);
    DCHECK(write_callback_.is_null());
    DCHECK_EQ(write_operation_, nullptr);

    auto hr = data_writer_->FlushAsync(&flush_operation_);
    if (FAILED(hr))
      return net::ERR_UNEXPECTED;

    flush_callback_ = std::move(callback);
    base::win::PostAsyncHandlers(
        flush_operation_.Get(),
        base::BindOnce(&DataWriterFileStreamWriter::OnFlushCompleted,
                       weak_factory_.GetWeakPtr()));
    return net::ERR_IO_PENDING;
  }

  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) final {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(flush_callback_.is_null());
    DCHECK_EQ(flush_operation_, nullptr);
    DCHECK(write_callback_.is_null());
    DCHECK_EQ(write_operation_, nullptr);

    // Before processing the Write request, increment the total number of file
    // bytes shared as part of the overall Share operation this belongs to, and
    // if it has exceeded the maximum allowed, abort writing to the streamed
    // file.
    file_bytes_shared_->data += buf_len;
    if (file_bytes_shared_->data > g_max_file_bytes)
      return net::ERR_UNEXPECTED;

    auto hr =
        data_writer_->WriteBytes(buf_len, reinterpret_cast<BYTE*>(buf->data()));
    if (FAILED(hr))
      return net::ERR_UNEXPECTED;

    hr = data_writer_->StoreAsync(&write_operation_);
    if (FAILED(hr))
      return net::ERR_UNEXPECTED;

    write_callback_ = std::move(callback);
    base::win::PostAsyncHandlers(
        write_operation_.Get(),
        base::BindOnce(&DataWriterFileStreamWriter::OnWriteCompleted,
                       weak_factory_.GetWeakPtr()));
    return net::ERR_IO_PENDING;
  }

 private:
  void OnFlushCompleted(boolean operation_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(!flush_callback_.is_null());
    DCHECK_NE(flush_operation_, nullptr);
    DCHECK(write_callback_.is_null());
    DCHECK_EQ(write_operation_, nullptr);

    flush_operation_.Reset();
    int result = operation_result == TRUE ? net::OK : net::ERR_UNEXPECTED;
    std::move(flush_callback_).Run(result);
  }

  void OnWriteCompleted(UINT32 operation_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(flush_callback_.is_null());
    DCHECK_EQ(flush_operation_, nullptr);
    DCHECK(!write_callback_.is_null());
    DCHECK_NE(write_operation_, nullptr);

    write_operation_.Reset();
    std::move(write_callback_).Run(operation_result);
  }

  ComPtr<IDataWriter> data_writer_;
  scoped_refptr<base::RefCountedData<uint64_t>> file_bytes_shared_;
  net::CompletionOnceCallback flush_callback_;
  ComPtr<IAsyncOperation<bool>> flush_operation_;
  net::CompletionOnceCallback write_callback_;
  ComPtr<IAsyncOperation<UINT32>> write_operation_;
  base::WeakPtrFactory<DataWriterFileStreamWriter> weak_factory_{this};
};

// Represents an ongoing operation of writing to an IOutputStream.
class OutputStreamWriteOperation
    : public base::RefCounted<OutputStreamWriteOperation> {
 public:
  OutputStreamWriteOperation(
      content::BrowserContext::BlobContextGetter blob_context_getter,
      scoped_refptr<base::RefCountedData<uint64_t>> file_bytes_shared,
      std::string uuid)
      : blob_context_getter_(blob_context_getter),
        file_bytes_shared_(file_bytes_shared),
        uuid_(uuid) {}

  // Begins the write operation on the |stream|, maintaining a reference to the
  // |stream| until the operation is completed, at which point it will be closed
  // (if possible) and the |on_complete| callback will be invoked. The caller
  // is still responsible for the lifetime of this object, but not of the
  // |stream|.
  void WriteStream(IOutputStream* stream,
                   base::OnceCallback<void()> on_complete) {
    stream_ = ComPtr<IOutputStream>(stream);
    on_complete_ = std::move(on_complete);
    if (!content::GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&OutputStreamWriteOperation::WriteStreamOnIOThread,
                           weak_factory_.GetWeakPtr())))
      Complete();
  }

 private:
  friend class base::RefCounted<OutputStreamWriteOperation>;

  ~OutputStreamWriteOperation() = default;

  void WriteStreamOnIOThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    storage::BlobStorageContext* blob_storage_context =
        blob_context_getter_.Run().get();
    if (!blob_storage_context) {
      Complete();
      return;
    }

    blob_handle_ = blob_storage_context->GetBlobDataFromUUID(uuid_);

    ComPtr<IDataWriterFactory> data_writer_factory;
    auto hr =
        GetActivationFactory<IDataWriterFactory,
                             RuntimeClass_Windows_Storage_Streams_DataWriter>(
            &data_writer_factory);
    if (FAILED(hr)) {
      Complete();
      return;
    }

    ComPtr<IDataWriter> data_writer;
    hr = data_writer_factory->CreateDataWriter(stream_.Get(), &data_writer);
    if (FAILED(hr)) {
      Complete();
      return;
    }

    writer_delegate_ = std::make_unique<storage::FileWriterDelegate>(
        std::make_unique<DataWriterFileStreamWriter>(std::move(data_writer),
                                                     file_bytes_shared_),
        storage::FlushPolicy::FLUSH_ON_COMPLETION);
    writer_delegate_->Start(
        blob_handle_->CreateReader(),
        base::BindRepeating(&OutputStreamWriteOperation::OnFileWritten,
                            weak_factory_.GetWeakPtr()));
  }

  void OnFileWritten(
      base::File::Error error,
      int64_t bytes_wrriten,
      storage::FileWriterDelegate::WriteProgressStatus write_status) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Any status other than SUCCESS_IO_PENDING indicates completion.
    if (write_status !=
        storage::FileWriterDelegate::WriteProgressStatus::SUCCESS_IO_PENDING) {
      Complete();
    }
  }

  void Complete() {
    // If the IOutputStream implements IClosable (e.g. the OutputStream class),
    // close the stream whenever we are done with this operation, regardless of
    // the outcome.
    if (stream_) {
      ComPtr<IClosable> closable;
      if (SUCCEEDED(stream_.As(&closable)))
        closable->Close();
    }

    std::move(on_complete_).Run();
  }

  content::BrowserContext::BlobContextGetter blob_context_getter_;
  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  scoped_refptr<base::RefCountedData<uint64_t>> file_bytes_shared_;
  base::OnceCallback<void()> on_complete_;
  ComPtr<IOutputStream> stream_;
  const std::string uuid_;
  std::unique_ptr<storage::FileWriterDelegate> writer_delegate_;
  base::WeakPtrFactory<OutputStreamWriteOperation> weak_factory_{this};
};
}  // namespace

// static
void ShareOperation::SetMaxFileBytesForTesting(uint64_t max_file_bytes) {
  g_max_file_bytes = max_file_bytes;
}

// static
void ShareOperation::SetRoGetActivationFactoryFunctionForTesting(
    decltype(&base::win::RoGetActivationFactory) value) {
  g_ro_get_activation_factory_function = value;
}

ShareOperation::ShareOperation(const std::string& title,
                               const std::string& text,
                               const GURL& url,
                               std::vector<blink::mojom::SharedFilePtr> files,
                               content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()),
      title_(std::move(title)),
      text_(std::move(text)),
      url_(std::move(url)),
      files_(std::move(files)) {}

ShareOperation::~ShareOperation() {
  if (callback_)
    Complete(blink::mojom::ShareError::CANCELED);
}

base::WeakPtr<ShareOperation> ShareOperation::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ShareOperation::Run(blink::mojom::ShareService::ShareCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  // If the corresponding web_contents have already been cleaned up, cancel
  // the operation.
  if (!web_contents_) {
    Complete(blink::mojom::ShareError::CANCELED);
    return;
  }

  if (files_.size() > 0) {
    // Determine the source for use with the OS IAttachmentExecute.
    // If the source cannot be determined, does not appear to be valid,
    // or is longer than the max length supported by the IAttachmentExecute
    // service, use a generic value that reliably maps to the Internet zone.
    GURL source_url = web_contents_->GetLastCommittedURL();
    std::wstring source = (source_url.is_valid() &&
                           source_url.spec().size() <= INTERNET_MAX_URL_LENGTH)
                              ? base::UTF8ToWide(source_url.spec())
                              : L"about:internet";

    // For each "file", check against the OS that it is allowed
    // The same instance cannot be used to check multiple files, so this
    // makes a new one per-file. For more details on this functionality, see
    // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iattachmentexecute-checkpolicy
    for (auto& file : files_) {
      ComPtr<IAttachmentExecute> attachment_services;
      if (FAILED(CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&attachment_services)))) {
        Complete(blink::mojom::ShareError::INTERNAL_ERROR);
        return;
      }
      if (FAILED(attachment_services->SetSource(source.c_str()))) {
        Complete(blink::mojom::ShareError::INTERNAL_ERROR);
        return;
      }
      if (FAILED(attachment_services->SetFileName(
              file->name.path().value().c_str()))) {
        Complete(blink::mojom::ShareError::INTERNAL_ERROR);
        return;
      }
      if (FAILED(attachment_services->CheckPolicy())) {
        Complete(blink::mojom::ShareError::PERMISSION_DENIED);
        return;
      }
    }
  }

  HWND hwnd =
      views::HWNDForNativeWindow(web_contents_->GetTopLevelNativeWindow());

  // Attempt to fetch the special HWND maintained for the primary WebContents of
  // this window. For the sake of better communication with screen readers this
  // HWND is (virtually) scoped to the same space as the WebContents (rather
  // than the entire actual window), so allows the resulting Share dialog to
  // better position/associate itself with the WebContents.
  //
  // Note: Though this is exposed to accessibility tools via standardized routes
  // we could expect to leverage here, the browser may choose to not set up all
  // these routes until an accessibility tool has been detected. Instead we look
  // for this specific class directly so we can find it even if accessibility
  // has not been configured yet.
  if (hwnd) {
    HWND accessible_hwnd =
        ::FindWindowExW(/*hWndParent*/ hwnd, /*hWndChildAfter*/ NULL,
                        /*lpszClass*/ ui::kLegacyRenderWidgetHostHwnd,
                        /*lpszWindow*/ NULL);
    if (accessible_hwnd) {
      hwnd = accessible_hwnd;
    }
  }

  show_share_ui_for_window_operation_ =
      std::make_unique<ShowShareUIForWindowOperation>(hwnd);
  show_share_ui_for_window_operation_->Run(base::BindOnce(
      &ShareOperation::OnDataRequested, weak_factory_.GetWeakPtr()));
}

void ShareOperation::OnDataRequested(IDataRequestedEventArgs* event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  blink::mojom::ShareError share_result;
  if (!web_contents_) {
    share_result = blink::mojom::ShareError::CANCELED;
  } else if (PutShareContentInEventArgs(event_args)) {
    share_result = blink::mojom::ShareError::OK;
  } else {
    share_result = blink::mojom::ShareError::INTERNAL_ERROR;
  }

  // If the share operation failed or is not being deferred, mark it as complete
  if (share_result != blink::mojom::ShareError::OK || !data_request_deferral_)
    Complete(share_result);
}

bool ShareOperation::PutShareContentInEventArgs(
    IDataRequestedEventArgs* event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!event_args)
    return false;

  ComPtr<IDataRequest> data_request;
  if (FAILED(event_args->get_Request(&data_request)))
    return false;

  if (FAILED(data_request->get_Data(&data_package_)))
    return false;

  ComPtr<IDataPackagePropertySet> data_prop_sets;
  if (FAILED(data_package_->get_Properties(&data_prop_sets)))
    return false;

  // Title is a required property for the UWP Share contract, so
  // if the provided title is empty we instead use a blank value.
  // https://docs.microsoft.com/en-us/windows/uwp/app-to-app/share-data
  base::win::ScopedHString title_h =
      base::win::ScopedHString::Create(title_.empty() ? " " : title_.c_str());
  if (FAILED(data_prop_sets->put_Title(title_h.get())))
    return false;

  return PutShareContentInDataPackage(data_request.Get());
}

bool ShareOperation::PutShareContentInDataPackage(IDataRequest* data_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!text_.empty()) {
    auto text_h = base::win::ScopedHString::Create(text_);
    if (FAILED(data_package_->SetText(text_h.get())))
      return false;
  }

  if (!url_.spec().empty()) {
    ComPtr<IUriRuntimeClassFactory> uri_factory;
    auto hr =
        GetActivationFactory<IUriRuntimeClassFactory,
                             RuntimeClass_Windows_Foundation_Uri>(&uri_factory);
    if (FAILED(hr))
      return hr;

    auto url_h = base::win::ScopedHString::Create(url_.spec().c_str());
    ComPtr<IUriRuntimeClass> uri;
    if (FAILED(uri_factory->CreateUri(url_h.get(), &uri)))
      return false;

    ComPtr<IDataPackage2> data_package_2;
    if (FAILED(data_package_.As(&data_package_2)))
      return false;

    if (FAILED(data_package_2->SetWebLink(uri.Get())))
      return false;
  }

  if (!files_.empty()) {
    // Fetch a deferral to allow for async operations
    if (FAILED(data_request->GetDeferral(&data_request_deferral_)))
      return false;

    // Initialize the output collection for the async operation(s)
    storage_items_ = Make<base::win::Vector<IStorageItem*>>();

    // Create a variable to be shared between all the operations processing the
    // blobs to streams. This will be used to keep a running count of total file
    // bytes shared as part of this Share operation so that if the maximum
    // allowed is exceeded the processing can be halted. Currently the
    // ShareOperation class is not guaranteed to outlive these operations, but
    // if that changes in the future it may be appropriate to make this a member
    // of the ShareOperation that is shared only be reference.
    auto file_bytes_shared =
        base::MakeRefCounted<base::RefCountedData<uint64_t>>(0);

    ComPtr<IStorageFileStatics> storage_statics;
    auto hr = GetActivationFactory<IStorageFileStatics,
                                   RuntimeClass_Windows_Storage_StorageFile>(
        &storage_statics);
    if (FAILED(hr))
      return false;

    for (auto& file : files_) {
      // This operation for converting the corresponding blob to a stream is
      // maintained as a scoped_refptr because it may out live this
      // ShareOperation instance. It is only invoked when the user has chosen a
      // Share target and that target decides to start reading the contents of
      // the corresponding IStorageFile. See
      // https://docs.microsoft.com/en-us/uwp/api/windows.storage.storagefile.createstreamedfileasync
      // If in the future the ShareOperation class is changed to live until the
      // target app has finished fully processing the shared content this could
      // be updated to be owned/maintained by this ShareOperation instance.
      auto operation = base::MakeRefCounted<OutputStreamWriteOperation>(
          web_contents_->GetBrowserContext()->GetBlobStorageContext(),
          file_bytes_shared, file->blob->uuid);
      auto name_h = base::win::ScopedHString::Create(file->name.path().value());
      auto raw_data_requested_callback =
          Callback<IStreamedFileDataRequestedHandler>(
              [operation](IOutputStream* stream) -> HRESULT {
                // No additional work is needed when the write has been
                // completed, but a callback is created to hold a reference
                // to the |operation| until the operation has completed.
                operation->WriteStream(
                    stream,
                    base::BindOnce(
                        [](scoped_refptr<OutputStreamWriteOperation>) {},
                        operation));
                return S_OK;
              });
      // The Callback function may return null in the E_OUTOFMEMORY case
      if (!raw_data_requested_callback)
        return false;
      ComPtr<IAsyncOperation<StorageFile*>> async_operation;
      if (FAILED(storage_statics->CreateStreamedFileAsync(
              name_h.get(), raw_data_requested_callback.Get(),
              /*thumbnail*/ nullptr, &async_operation))) {
        return false;
      }

      async_operations_.push_back(async_operation);

      if (FAILED(base::win::PostAsyncHandlers(
              async_operation.Get(),
              base::BindOnce(&ShareOperation::OnStreamedFileCreated,
                             weak_factory_.GetWeakPtr())))) {
        return false;
      }
    }
  }

  return true;
}

void ShareOperation::OnStreamedFileCreated(ComPtr<IStorageFile> storage_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If there is no callback this ShareOperation already completed due to an
  // error, so work can be halted early.
  if (!callback_)
    return;

  if (!storage_file) {
    Complete(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  ComPtr<IStorageItem> storage_item;
  if (FAILED(storage_file.As(&storage_item))) {
    Complete(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  if (FAILED(storage_items_->Append(storage_item.Get()))) {
    Complete(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  unsigned int size;
  if (FAILED(storage_items_->get_Size(&size))) {
    Complete(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  // If this is not the final file, no more work to do
  if (size != files_.size())
    return;

  if (FAILED(data_package_->SetStorageItems(storage_items_.Get(),
                                            true /*readonly*/))) {
    Complete(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  data_request_deferral_->Complete();
  Complete(blink::mojom::ShareError::OK);
  return;
}

void ShareOperation::Complete(const blink::mojom::ShareError share_result) {
  std::move(callback_).Run(share_result);
}

}  // namespace webshare
