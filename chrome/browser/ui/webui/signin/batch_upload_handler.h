// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/types/id_type.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "components/sync/service/local_data_description.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

struct AccountInfo;

class Browser;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
namespace device_reauth {
class DeviceAuthenticator;
}  // namespace device_reauth
#endif

// WebUI message handler for the Batch Upload dialog bubble.
class BatchUploadHandler : public batch_upload::mojom::PageHandler {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  BatchUploadHandler(
      mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver,
      mojo::PendingRemote<batch_upload::mojom::Page> page,
      const AccountInfo& account_info,
      Browser* browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      base::RepeatingCallback<void(int)> update_view_height_callback,
      base::RepeatingCallback<void(bool)> allow_web_view_input_callback,
      BatchUploadSelectedDataTypeItemsCallback completion_callback);
  ~BatchUploadHandler() override;

  BatchUploadHandler(const BatchUploadHandler&) = delete;
  BatchUploadHandler& operator=(const BatchUploadHandler&) = delete;

  // batch_upload::mojom::PageHandler:
  void UpdateViewHeight(uint32_t height) override;
  // The order of the input vector `ids_to_move` matches with the order of
  // `local_data_description_list_`.
  void SaveToAccount(
      const std::vector<std::vector<int32_t>>& ids_to_move) override;
  void Close() override;

  static int GetTypeSectionTitleId(syncer::DataType type);

 private:
  // Strong Alias ID which is reprenseted as an int.
  using InternalId = base::IdType32<BatchUploadHandler>;

  // Construct the `BatchUploadData` structure to be used in the Ui. Combining
  // the account info, dialog subtitle and a list of data containers.
  // The Data contaiers are a list items to be shown on the batch upload ui.
  // Sending the information using the Mojo equivalent structures:
  // `syncer::LocalDataDescription` -> `batch_upload::mojom::DataContainer`
  // `BatchUploadDataItem` -> `batch_upload::mojom::DataItem`
  // Also populates the `internal_data_item_id_mapping_list_` for each data
  // item per section.
  batch_upload::mojom::BatchUploadDataPtr ConstructMojoBatchUploadData(
      const AccountInfo& account_info);

  // Callback to be used after verififying that saving to account is allowed.
  void OnSaveToAccountRequestReady(
      std::map<syncer::DataType,
               std::vector<syncer::LocalDataItemModel::DataId>> ids_to_move,
      bool allowed);

  raw_ref<Browser> browser_;
  std::vector<syncer::LocalDataDescription> local_data_description_list_;
  base::RepeatingCallback<void(int)> update_view_height_callback_;
  base::RepeatingCallback<void(bool)> allow_web_view_input_callback_;
  BatchUploadSelectedDataTypeItemsCallback completion_callback_;

  // Internal Id mapping used to map items from their real
  // `BatchUploadDataItemModel::DataId` (which can be of different types) to the
  // id given to the Ui through Mojo (which is a fixed type). The vector size
  // matches the `local_data_description_list_` size and positions.
  // The mapping is needed for getting back the real ids when receiving the
  // result from Mojo in `SaveToAccount()`.
  std::vector<std::map<InternalId, syncer::LocalDataItemModel::DataId>>
      internal_data_item_id_mapping_list_;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Allows to trigger the reauth, this needs to be a member field since the
  // verification is asynchronous and the object needs to live until the
  // response.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;
#endif

  // Allows handling received messages from the web ui page.
  mojo::Receiver<batch_upload::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  mojo::Remote<batch_upload::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_HANDLER_H_
