// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/batch_upload_handler.h"

#include <algorithm>
#include <iterator>

#include "base/strings/to_string.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_authenticator.h"
#endif

namespace {

// The subtitle of the dialog depends on which type of data is shown and the
// number of different types.
// If the first type represents a "hero" type, it will have a specific treatment
// with a different string. In addition if other types exists as well, it will
// add to its string the "and other items" part. Currently the only "hero" type
// available is passwords.
// If the first type is not a "hero" type, then simply items will be mentioned
// in the subtitle.
// All strings take into account the plural aspect based on the count. The count
// is either the "hero" type count if applicable, or the total item count in the
// generic case.
std::string ComputeBatchUploadSubtitle(syncer::DataType first_type,
                                       size_t first_type_item_count,
                                       size_t number_of_types,
                                       size_t total_item_count) {
  // Check for the "hero" type availability.
  if (first_type == syncer::DataType::PASSWORDS) {
    if (number_of_types > 1) {
      // Returns "passwords + other items" combo string.
      return l10n_util::GetPluralStringFUTF8(
          IDS_BATCH_UPLOAD_SUBTITLE_DESCRIPTION_PASSWORDS_COMBO,
          first_type_item_count);
    }
    // Returns the passwords only string.
    return l10n_util::GetPluralStringFUTF8(
        IDS_BATCH_UPLOAD_SUBTITLE_DESCRIPTION_PASSWORDS, first_type_item_count);
  }

  // Returns the generic items string.
  return l10n_util::GetPluralStringFUTF8(
      IDS_BATCH_UPLOAD_SUBTITLE_DESCRIPTION_ITEMS, total_item_count);
}

}  // namespace

BatchUploadHandler::BatchUploadHandler(
    mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver,
    mojo::PendingRemote<batch_upload::mojom::Page> page,
    const AccountInfo& account_info,
    Browser* browser,
    std::vector<syncer::LocalDataDescription> local_data_description_list,
    base::RepeatingCallback<void(int)> update_view_height_callback,
    base::RepeatingCallback<void(bool)> allow_web_view_input_callback,
    BatchUploadSelectedDataTypeItemsCallback completion_callback)
    : browser_(*browser),
      local_data_description_list_(std::move(local_data_description_list)),
      update_view_height_callback_(update_view_height_callback),
      allow_web_view_input_callback_(allow_web_view_input_callback),
      completion_callback_(std::move(completion_callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  page_->SendBatchUploadData(ConstructMojoBatchUploadData(account_info));
}

BatchUploadHandler::~BatchUploadHandler() = default;

void BatchUploadHandler::UpdateViewHeight(uint32_t height) {
  update_view_height_callback_.Run(height);
}

void BatchUploadHandler::Close() {
  // Clear the data as after `completion_callback_` is done.
  local_data_description_list_.clear();
  std::move(completion_callback_).Run({});
}

void BatchUploadHandler::SaveToAccount(
    const std::vector<std::vector<int32_t>>& ids_to_move) {
  CHECK_EQ(ids_to_move.size(), local_data_description_list_.size());
  CHECK_EQ(ids_to_move.size(), internal_data_item_id_mapping_list_.size());

  // Convert `ids_to_move` ids from `int32_t` to
  // `LocalDataItemModel::DataId` using
  // `internal_data_item_id_mapping_list_`.
  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      ret_ids_to_move;
  for (size_t i = 0; i < ids_to_move.size(); ++i) {
    const std::map<InternalId, syncer::LocalDataItemModel::DataId>&
        internal_data_item_id_mapping = internal_data_item_id_mapping_list_[i];
    CHECK_LE(ids_to_move[i].size(), internal_data_item_id_mapping.size());

    std::vector<syncer::LocalDataItemModel::DataId> section_ids;
    std::ranges::transform(
        ids_to_move[i], std::back_inserter(section_ids),
        [&internal_data_item_id_mapping](int32_t id) {
          BatchUploadHandler::InternalId internal_item_id =
              BatchUploadHandler::InternalId(id);
          CHECK(internal_data_item_id_mapping.contains(internal_item_id));
          return internal_data_item_id_mapping.at(internal_item_id);
        });
    ret_ids_to_move.insert_or_assign(local_data_description_list_[i].type,
                                     section_ids);
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Only show reauth if passwords are selected to be saved. Passwords is the
  // only data type that needs authentication.
  auto it = ret_ids_to_move.find(syncer::PASSWORDS);
  bool should_show_reauth = it != ret_ids_to_move.end() && !it->second.empty();
#endif

  base::OnceCallback<void(bool)> on_save_to_account_ready =
      base::BindOnce(&BatchUploadHandler::OnSaveToAccountRequestReady,
                     base::Unretained(this), std::move(ret_ids_to_move));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (should_show_reauth) {
    // Disable all inputs in the view during reauth.
    allow_web_view_input_callback_.Run(false);

    device_reauth::DeviceAuthParams params(
        base::Seconds(0),
        device_reauth::DeviceAuthSource::kSettingsBatchUpload);

    device_authenticator_ = ChromeDeviceAuthenticatorFactory::GetForProfile(
        browser_->profile(),
        browser_->tab_strip_model()
            ->GetActiveWebContents()
            ->GetTopLevelNativeWindow(),
        params);

    // Show authentication before proceeding with the save to account.
    device_authenticator_->AuthenticateWithMessage(
        l10n_util::GetStringUTF16(
            IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX),
        std::move(on_save_to_account_ready));
    return;
  }
#endif

  // Proceed directly without reauth and allowing data to be saved.
  std::move(on_save_to_account_ready).Run(true);
}

void BatchUploadHandler::OnSaveToAccountRequestReady(
    std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
        ids_to_move,
    bool allowed) {
  // Reset the view inputs in all cases.
  allow_web_view_input_callback_.Run(true);

  // If not allowed do not proceed, but do not close the dialog as well to give
  // another opportunity to the user without losing his choices.
  if (!allowed) {
    return;
  }

  local_data_description_list_.clear();
  std::move(completion_callback_).Run(std::move(ids_to_move));
}

batch_upload::mojom::BatchUploadDataPtr
BatchUploadHandler::ConstructMojoBatchUploadData(
    const AccountInfo& account_info) {
  CHECK(!account_info.IsEmpty());
  CHECK(!local_data_description_list_.empty());

  batch_upload::mojom::BatchUploadAccountInfoPtr account_info_mojo =
      batch_upload::mojom::BatchUploadAccountInfo::New();
  account_info_mojo->email = account_info.email;
  account_info_mojo->data_picture_url =
      signin::GetAccountPictureUrl(account_info);

  size_t total_item_count = 0;
  std::vector<batch_upload::mojom::DataContainerPtr> data_containers_mojo;
  for (const auto& local_data_description : local_data_description_list_) {
    CHECK(!local_data_description.local_data_models.empty());

    std::map<InternalId, syncer::LocalDataItemModel::DataId>&
        internal_data_item_id_mapping =
            internal_data_item_id_mapping_list_.emplace_back();

    batch_upload::mojom::DataContainerPtr data_container_mojo =
        batch_upload::mojom::DataContainer::New();
    // TODO(crbug.com/372450941): Adadpt the mojo variable name.
    data_container_mojo->section_title =
        base::ToString(GetTypeSectionTitleId(local_data_description.type));

    InternalId current_id = InternalId(0);
    for (const auto& data_item : local_data_description.local_data_models) {
      batch_upload::mojom::DataItemPtr data_item_mojo =
          batch_upload::mojom::DataItem::New();
      // Increment the Id everytime to have a unique id per data type.
      current_id = BatchUploadHandler::InternalId(current_id.value() + 1);
      // Store the internal Id mapping to the real item id and use the internal
      // mapping in the Mojo model.
      internal_data_item_id_mapping.insert_or_assign(current_id, data_item.id);
      data_item_mojo->id = current_id.value();
      data_item_mojo->icon_url = data_item.icon_url.spec();
      data_item_mojo->title = data_item.title;
      data_item_mojo->subtitle = data_item.subtitle;

      data_container_mojo->data_items.push_back(std::move(data_item_mojo));
    }
    data_containers_mojo.push_back(std::move(data_container_mojo));

    total_item_count += local_data_description.local_data_models.size();
  }

  batch_upload::mojom::BatchUploadDataPtr batch_upload_mojo =
      batch_upload::mojom::BatchUploadData::New();

  batch_upload_mojo->account_info = std::move(account_info_mojo);
  batch_upload_mojo->dialog_subtitle = ComputeBatchUploadSubtitle(
      /*first_type=*/local_data_description_list_[0].type,
      /*first_type_item_count=*/
      local_data_description_list_[0].local_data_models.size(),
      local_data_description_list_.size(), total_item_count);
  batch_upload_mojo->data_containers = std::move(data_containers_mojo);

  return batch_upload_mojo;
}

int BatchUploadHandler::GetTypeSectionTitleId(syncer::DataType type) {
  switch (type) {
    case syncer::DataType::PASSWORDS:
      return IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS;
    case syncer::DataType::CONTACT_INFO:
      return IDS_BATCH_UPLOAD_SECTION_TITLE_ADDRESSES;
    default:
      NOTREACHED();
  }
}
