// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/webshare/mac/sharing_service_operation.h"

#include <AppKit/AppKit.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visibility_timer_tab_helper.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "chrome/browser/webshare/prepare_subdirectory_task.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/store_files_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
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
  std::string unique_subdirectory = base::StringPrintf(
      "share-%s", base::Uuid::GenerateRandomV4().AsLowercaseString().c_str());
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
    : web_contents_(web_contents->GetWeakPtr()),
      title_(title),
      text_(text),
      url_(url),
      shared_files_(std::move(files)) {}

SharingServiceOperation::~SharingServiceOperation() = default;

void SharingServiceOperation::Share(
    blink::mojom::ShareService::ShareCallback callback) {
  callback_ = std::move(callback);

  Profile* const profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  DCHECK(profile);

  // File sharing is denied in incognito, as files are written to disk.
  // To prevent sites from using that to detect whether incognito mode is
  // active, we deny after a random time delay, to simulate a user cancelling
  // the share.
  if (profile->IsIncognitoProfile() && !shared_files_.empty()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents_.get());
    VisibilityTimerTabHelper::FromWebContents(web_contents_.get())
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(std::move(callback_),
                           blink::mojom::ShareError::CANCELED),
            base::Seconds(delay_seconds));
    return;
  }

  if (shared_files_.size() == 0) {
    GetSharePickerCallback().Run(
        web_contents_.get(), file_paths_, text_, title_, url_,
        base::BindOnce(&SharingServiceOperation::OnShowSharePicker,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  BrowserContext* browser_context = web_contents_->GetBrowserContext();
  StoragePartition* const partition =
      browser_context->GetDefaultStoragePartition();
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
  if (!web_contents_ || error != blink::mojom::ShareError::OK) {
    std::move(callback_).Run(error);
    return;
  }

  for (const auto& file : shared_files_) {
    // SafeBaseName protects against including paths in a file name.
    std::string file_name = file->name.path().value();
    DCHECK_EQ(file_name.find('/'), std::string::npos);
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
  if (!web_contents_ || error != blink::mojom::ShareError::OK) {
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
  if (!web_contents_ || error != blink::mojom::ShareError::OK) {
    PrepareDirectoryTask::ScheduleSharedFileDeletion(std::move(file_paths_),
                                                     base::Minutes(0));
    std::move(callback_).Run(error);
    return;
  }

  GetSharePickerCallback().Run(
      web_contents_.get(), file_paths_, text_, title_, url_,
      base::BindOnce(&SharingServiceOperation::OnShowSharePicker,
                     weak_factory_.GetWeakPtr()));
}

void SharingServiceOperation::OnShowSharePicker(
    blink::mojom::ShareError error) {
  if (file_paths_.size() > 0) {
    PrepareDirectoryTask::ScheduleSharedFileDeletion(std::move(file_paths_),
                                                     base::Minutes(0));
  }
  std::move(callback_).Run(error);
}

// static
void SharingServiceOperation::ShowSharePicker(
    content::WebContents* web_contents,
    const std::vector<base::FilePath>& file_paths,
    const std::string& text,
    const std::string& title,
    const GURL& url,
    blink::mojom::ShareService::ShareCallback callback) {
  std::vector<std::string> file_paths_as_utf8;
  for (const auto& file_path : file_paths) {
    file_paths_as_utf8.emplace_back(file_path.AsUTF8Unsafe());
  }

  web_contents->GetRenderWidgetHostView()->ShowSharePicker(
      title, text, url.spec(), file_paths_as_utf8, std::move(callback));
}

// static
SharingServiceOperation::SharePickerCallback&
SharingServiceOperation::GetSharePickerCallback() {
  static base::NoDestructor<SharePickerCallback> callback(
      base::BindRepeating(&SharingServiceOperation::ShowSharePicker));
  return *callback;
}

}  // namespace webshare
