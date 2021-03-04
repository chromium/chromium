// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/webshare/mac/sharing_service_operation.h"

#include <AppKit/AppKit.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/i18n/file_util_icu.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "chrome/browser/webshare/prepare_subdirectory_task.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/store_files_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::StoragePartition;

namespace {

constexpr base::FilePath::CharType kWebShareDirname[] =
    FILE_PATH_LITERAL("WebShare");

base::FilePath GenerateUniqueSubDirectory(const base::FilePath& directory) {
  std::string unique_subdirectory =
      base::StringPrintf("share-%s", base::GenerateGUID().c_str());
  return directory.Append(unique_subdirectory);
}

}  // namespace

namespace webshare {

SharingServiceOperation::SharingServiceOperation(
    const std::string& title,
    const std::string& text,
    const GURL& url,
    std::vector<blink::mojom::SharedFilePtr> files,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      title_(title),
      text_(text),
      url_(url),
      shared_files_(std::move(files)) {}

SharingServiceOperation::~SharingServiceOperation() = default;

void SharingServiceOperation::Share(
    blink::mojom::ShareService::ShareCallback callback) {
  callback_ = std::move(callback);

  if (shared_files_.size() == 0) {
    GetSharePickerCallback().Run(
        web_contents(), file_paths_, text_, title_,
        base::BindOnce(&SharingServiceOperation::OnShowSharePicker,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  StoragePartition* const partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  directory_ = partition->GetPath().Append(kWebShareDirname);

  prepare_directory_task_ = std::make_unique<PrepareDirectoryTask>(
      directory_, kMaxSharedFileBytes,
      base::BindOnce(&SharingServiceOperation::OnPrepareDirectory,
                     weak_factory_.GetWeakPtr()));

  prepare_directory_task_->Start();
}

// static
void SharingServiceOperation::SetSharePickerCallbackForTesting(
    SharePickerCallback callback) {
  GetSharePickerCallback() = std::move(callback);
}

void SharingServiceOperation::OnPrepareDirectory(
    blink::mojom::ShareError error) {
  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(callback_).Run(error);
    return;
  }

  for (const auto& file : shared_files_) {
    std::string file_name = file->name;
    base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
    file_paths_.push_back(
        GenerateUniqueSubDirectory(directory_).Append(file_name));
  }

  prepare_subdirectory_task_ = std::make_unique<PrepareSubDirectoryTask>(
      file_paths_,
      base::BindOnce(&SharingServiceOperation::OnPrepareSubDirectory,
                     weak_factory_.GetWeakPtr()));

  prepare_subdirectory_task_->Start();
}

void SharingServiceOperation::OnPrepareSubDirectory(
    blink::mojom::ShareError error) {
  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(callback_).Run(error);
    return;
  }

  auto store_files_task = std::make_unique<StoreFilesTask>(
      file_paths_, std::move(shared_files_), kMaxSharedFileBytes,
      base::BindOnce(&SharingServiceOperation::OnStoreFiles,
                     weak_factory_.GetWeakPtr()));

  // The StoreFilesTask is self-owned.
  store_files_task.release()->Start();
}

void SharingServiceOperation::OnStoreFiles(blink::mojom::ShareError error) {
  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(callback_).Run(error);
    return;
  }

  GetSharePickerCallback().Run(
      web_contents(), file_paths_, text_, title_,
      base::BindOnce(&SharingServiceOperation::OnShowSharePicker,
                     weak_factory_.GetWeakPtr()));
}

void SharingServiceOperation::OnShowSharePicker(
    blink::mojom::ShareError error) {
  std::move(callback_).Run(error);
}

// static
void SharingServiceOperation::ShowSharePicker(
    content::WebContents* web_contents,
    const std::vector<base::FilePath>& file_paths,
    const std::string& text,
    const std::string& title,
    blink::mojom::ShareService::ShareCallback callback) {
  std::vector<std::string> file_paths_as_utf8;
  for (const auto& file_path : file_paths) {
    file_paths_as_utf8.emplace_back(file_path.AsUTF8Unsafe());
  }

  // TODO(crbug.com/1144920): Add & invoke NSSharingServicePicker in
  // remote_cocoa
  std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
}

// static
SharingServiceOperation::SharePickerCallback&
SharingServiceOperation::GetSharePickerCallback() {
  static base::NoDestructor<SharePickerCallback> callback(
      base::BindRepeating(&SharingServiceOperation::ShowSharePicker));
  return *callback;
}

}  // namespace webshare
