// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/webshare/chromeos/prepare_directory_task.h"
#include "chrome/browser/webshare/chromeos/store_files_task.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"

using content::BrowserContext;
using content::BrowserThread;
using content::StoragePartition;
using content::WebContents;

namespace {

constexpr base::FilePath::CharType kWebShareDirname[] =
    FILE_PATH_LITERAL("WebShare");

// We don't use |supplied_name| as it may contain special characters, and it may
// not be unique. The suffix has been checked by
// ShareServiceImpl::IsDangerousFilename().
base::FilePath GenerateFileName(const base::FilePath& directory,
                                const std::string& supplied_name) {
  static unsigned counter = 0;

  ++counter;

  size_t suffix_pos = supplied_name.find_last_of('.');
  std::string filename = base::StringPrintf("share%u%s", counter,
                                            supplied_name.c_str() + suffix_pos);
  return directory.Append(filename);
}

blink::mojom::ShareError SharesheetResultToShareError(
    sharesheet::SharesheetResult result) {
  switch (result) {
    case sharesheet::SharesheetResult::kSuccess:
      return blink::mojom::ShareError::OK;
    case sharesheet::SharesheetResult::kCancel:
      return blink::mojom::ShareError::CANCELED;
  }
}

}  // namespace

namespace webshare {

SharesheetClient::CurrentShare::CurrentShare() = default;
SharesheetClient::CurrentShare::CurrentShare(CurrentShare&&) = default;
SharesheetClient::CurrentShare& SharesheetClient::CurrentShare::operator=(
    SharesheetClient::CurrentShare&&) = default;
SharesheetClient::CurrentShare::~CurrentShare() = default;

SharesheetClient::SharesheetClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SharesheetClient::~SharesheetClient() = default;

void SharesheetClient::Share(
    const std::string& title,
    const std::string& text,
    const GURL& share_url,
    std::vector<blink::mojom::SharedFilePtr> files,
    blink::mojom::ShareService::ShareCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The SharesheetClient only shows one share sheet at a time.
  if (current_share_.has_value() || !web_contents()) {
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  if (files.empty()) {
    // TODO(crbug.com/1127670): Support title/text/url sharing.
    std::move(callback).Run(blink::mojom::ShareError::CANCELED);
    return;
  }

  BrowserContext* const browser_context = web_contents()->GetBrowserContext();
  StoragePartition* const partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  current_share_ = CurrentShare();
  current_share_->files = std::move(files);
  current_share_->directory = partition->GetPath().Append(kWebShareDirname);
  current_share_->callback = std::move(callback);

  current_share_->prepare_directory_task =
      std::make_unique<PrepareDirectoryTask>(
          current_share_->directory,
          base::BindOnce(&SharesheetClient::OnPrepareDirectory,
                         weak_ptr_factory_.GetWeakPtr()));
  current_share_->prepare_directory_task->Start();
}

// static
void SharesheetClient::SetSharesheetCallbackForTesting(
    SharesheetCallback callback) {
  GetSharesheetCallback() = std::move(callback);
}

void SharesheetClient::OnPrepareDirectory(blink::mojom::ShareError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(current_share_->callback).Run(error);
    current_share_ = base::nullopt;
    return;
  }

  for (const auto& file : current_share_->files) {
    current_share_->content_types.push_back(file->blob->content_type);
    current_share_->file_paths.push_back(
        GenerateFileName(current_share_->directory, file->name));
  }

  BrowserContext* const browser_context = web_contents()->GetBrowserContext();
  std::unique_ptr<StoreFilesTask> store_files_task =
      std::make_unique<StoreFilesTask>(
          BrowserContext::GetBlobStorageContext(browser_context),
          current_share_->file_paths, std::move(current_share_->files),
          base::BindOnce(&SharesheetClient::OnStoreFiles,
                         weak_ptr_factory_.GetWeakPtr()));

  // The StoreFilesTask is self-owned.
  store_files_task.release()->Start();
}

void SharesheetClient::OnStoreFiles(blink::mojom::ShareError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(current_share_->callback).Run(error);
    current_share_ = base::nullopt;
    return;
  }

  GetSharesheetCallback().Run(
      web_contents(), std::move(current_share_->file_paths),
      std::move(current_share_->content_types),
      base::BindOnce(&SharesheetClient::OnShowSharesheet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetClient::OnShowSharesheet(sharesheet::SharesheetResult result) {
  std::move(current_share_->callback).Run(SharesheetResultToShareError(result));
  current_share_ = base::nullopt;
}

// static
void SharesheetClient::ShowSharesheet(content::WebContents* web_contents,
                                      std::vector<base::FilePath> file_paths,
                                      std::vector<std::string> content_types,
                                      CloseCallback close_callback) {
  if (!base::FeatureList::IsEnabled(features::kSharesheet)) {
    std::move(close_callback).Run(sharesheet::SharesheetResult::kCancel);
    return;
  }

  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);

  sharesheet_service->ShowBubble(
      web_contents,
      apps_util::CreateShareIntentFromFiles(profile, std::move(file_paths),
                                            std::move(content_types)),
      std::move(close_callback));
}

SharesheetClient::SharesheetCallback&
SharesheetClient::GetSharesheetCallback() {
  static base::NoDestructor<SharesheetCallback> callback(
      base::BindRepeating(&SharesheetClient::ShowSharesheet));

  return *callback;
}

void SharesheetClient::WebContentsDestroyed() {
  current_share_ = base::nullopt;
}

}  // namespace webshare
