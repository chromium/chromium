// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEBSHARE_WIN_SHARE_OPERATION_H_
#define CHROME_BROWSER_WEBSHARE_WIN_SHARE_OPERATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/win/core_winrt_util.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

#include <wrl/client.h>

namespace ABI::Windows {
namespace ApplicationModel::DataTransfer {
struct IDataPackage;
class IDataRequest;
class IDataRequestDeferral;
class IDataRequestedEventArgs;
}  // namespace ApplicationModel::DataTransfer
namespace Storage {
class IStorageFile;
class IStorageItem;
}  // namespace Storage
}  // namespace ABI::Windows

namespace base::win {
template <typename T>
class Vector;
}  // namespace base::win

namespace content {
class WebContents;
}  // namespace content

namespace webshare {

class ShowShareUIForWindowOperation;

class ShareOperation final {
 public:
  static void SetMaxFileBytesForTesting(uint64_t max_file_bytes);

  // Test hook for overriding the base RoGetActivationFactory function
  static void SetRoGetActivationFactoryFunctionForTesting(
      decltype(&base::win::RoGetActivationFactory) value);

  ShareOperation(const std::string& title,
                 const std::string& text,
                 const GURL& url,
                 std::vector<blink::mojom::SharedFilePtr> files,
                 content::WebContents* web_contents);
  ShareOperation(const ShareOperation&) = delete;
  ShareOperation& operator=(const ShareOperation&) = delete;
  ~ShareOperation();

  base::WeakPtr<ShareOperation> AsWeakPtr();

  // Starts this Windows Share operation for the previously provided content.
  // The |callback| will be invoked upon completion of the operation with a
  // value indicating the success of the operation, or if the returned instance
  // is destroyed before the operation is completed the |callback| will be
  // invoked with a CANCELLED value and the underlying Windows operation will be
  // aborted.
  void Run(blink::mojom::ShareService::ShareCallback callback);

 private:
  void OnDataRequested(
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs* e);
  bool PutShareContentInDataPackage(
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequest* data_request);
  bool PutShareContentInEventArgs(
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs* e);
  void OnStreamedFileCreated(
      Microsoft::WRL::ComPtr<ABI::Windows::Storage::IStorageFile> storage_file);
  void Complete(const blink::mojom::ShareError share_error);

  const base::WeakPtr<content::WebContents> web_contents_;
  const std::string title_;
  const std::string text_;
  const GURL url_;
  const std::vector<blink::mojom::SharedFilePtr> files_;

  std::vector<Microsoft::WRL::ComPtr<IUnknown>> async_operations_;
  blink::mojom::ShareService::ShareCallback callback_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::ApplicationModel::DataTransfer::IDataPackage>
      data_package_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::ApplicationModel::DataTransfer::IDataRequestDeferral>
      data_request_deferral_;
  std::unique_ptr<ShowShareUIForWindowOperation>
      show_share_ui_for_window_operation_;
  // Though this Vector is declared as using a raw IStorageItem*, because
  // IStorageItem implements IUnknown it will be stored internally with
  // ComPtrs, so does not need to be specially handled.
  Microsoft::WRL::ComPtr<
      base::win::Vector<ABI::Windows::Storage::IStorageItem*>>
      storage_items_;
  base::WeakPtrFactory<ShareOperation> weak_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_SHARE_OPERATION_H_
