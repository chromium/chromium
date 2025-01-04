// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/visibility_timer_tab_helper.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "chrome/browser/webshare/prepare_subdirectory_task.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/store_files_task.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"

using content::BrowserThread;
using content::WebContents;

namespace {

constexpr base::FilePath::CharType kWebShareDirname[] =
    FILE_PATH_LITERAL(".WebShare");

constexpr char kDefaultShareName[] = "share";

// Note that the suffix of |suggested_name| has been checked by
// ShareServiceImpl::IsDangerousFilename().
base::FilePath GenerateFileName(content::WebContents* web_contents,
                                const base::FilePath& directory,
                                const base::SafeBaseName& suggested_name) {
  static unsigned counter = 0;

  ++counter;
  std::string dirname = base::StringPrintf("share%u", counter);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::string referrer_charset =
      profile->GetPrefs()->GetString(prefs::kDefaultCharset);

  base::FilePath filename =
      net::GenerateFileName(web_contents->GetLastCommittedURL(),
                            /*content_disposition=*/std::string(),
                            referrer_charset, suggested_name.path().value(),
                            /*mime_type=*/std::string(), kDefaultShareName);

  return directory.Append(dirname).Append(filename);
}

blink::mojom::ShareError SharesheetResultToShareError(
    sharesheet::SharesheetResult result) {
  switch (result) {
    case sharesheet::SharesheetResult::kSuccess:
      return blink::mojom::ShareError::OK;
    case sharesheet::SharesheetResult::kCancel:
    case sharesheet::SharesheetResult::kErrorAlreadyOpen:
    case sharesheet::SharesheetResult::kErrorWindowClosed:
      return blink::mojom::ShareError::CANCELED;
  }
}

// Deletes immediate parent directories of specified |file_paths|, after waiting
// |delay|.
void ScheduleSharedFileDirectoryDeletion(std::vector<base::FilePath> file_paths,
                                         base::TimeDelta delay) {
  for (size_t i = 0; i < file_paths.size(); ++i)
    file_paths[i] = file_paths[i].DirName();

  webshare::PrepareDirectoryTask::ScheduleSharedFileDeletion(
      std::move(file_paths), delay);
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
    VLOG(1) << "Cannot share when an existing share is in progress, or after "
               "navigating away";
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  Profile* const profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DCHECK(profile);

  // File sharing is denied in incognito, as files are written to disk.
  // To prevent sites from using that to detect whether incognito mode is
  // active, we deny after a random time delay, to simulate a user cancelling
  // the share.
  if (profile->IsIncognitoProfile() && !files.empty()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents());
    VisibilityTimerTabHelper::FromWebContents(web_contents())
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           blink::mojom::ShareError::CANCELED),
            base::Seconds(delay_seconds));
    return;
  }

  current_share_ = CurrentShare();
  current_share_->files = std::move(files);
  current_share_->directory =
      file_manager::util::GetShareCacheFilePath(profile).Append(
          kWebShareDirname);
  if (share_url.is_valid()) {
    if (text.empty())
      current_share_->text = share_url.spec();
    else
      current_share_->text = text + " " + share_url.spec();
  } else {
    current_share_->text = text;
  }
  current_share_->title = title;
  current_share_->callback = std::move(callback);

  if (current_share_->files.empty()) {
    GetSharesheetCallback().Run(
        web_contents(), current_share_->file_paths,
        current_share_->content_types, current_share_->file_sizes,
        current_share_->text, current_share_->title,
        base::BindOnce(&SharesheetClient::OnShowSharesheet,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Previously, shared files were stored in MyFiles/.WebShare. We remove this
  // obsolete directory.
  PrepareDirectoryTask::ScheduleSharedFileDeletion(
      {file_manager::util::GetMyFilesFolderForProfile(profile).Append(
          kWebShareDirname)},
      /*delay=*/base::TimeDelta());

  current_share_->prepare_directory_task =
      std::make_unique<PrepareDirectoryTask>(
          current_share_->directory, kMaxSharedFileBytes,
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
  if (!current_share_.has_value())
    return;

  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(current_share_->callback).Run(error);
    current_share_ = std::nullopt;
    return;
  }

  for (const auto& file : current_share_->files) {
    current_share_->content_types.push_back(file->blob->content_type);
    current_share_->file_paths.push_back(GenerateFileName(
        web_contents(), current_share_->directory, file->name));
    current_share_->file_sizes.push_back(file->blob->size);
  }

  current_share_->prepare_subdirectory_task =
      std::make_unique<PrepareSubDirectoryTask>(
          current_share_->file_paths,
          base::BindOnce(&SharesheetClient::OnPrepareSubdirectory,
                         weak_ptr_factory_.GetWeakPtr()));
  current_share_->prepare_subdirectory_task->Start();
}

void SharesheetClient::OnPrepareSubdirectory(blink::mojom::ShareError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!current_share_.has_value())
    return;

  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(current_share_->callback).Run(error);
    current_share_ = std::nullopt;
    return;
  }

  std::unique_ptr<StoreFilesTask> store_files_task =
      std::make_unique<StoreFilesTask>(
          current_share_->file_paths, std::move(current_share_->files),
          kMaxSharedFileBytes,
          base::BindOnce(&SharesheetClient::OnStoreFiles,
                         weak_ptr_factory_.GetWeakPtr()));

  // The StoreFilesTask is self-owned.
  store_files_task.release()->Start();
}

void SharesheetClient::OnStoreFiles(blink::mojom::ShareError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!current_share_.has_value())
    return;

  if (!web_contents() || error != blink::mojom::ShareError::OK) {
    std::move(current_share_->callback).Run(error);
    ScheduleSharedFileDirectoryDeletion(std::move(current_share_->file_paths),
                                        base::Minutes(0));
    current_share_ = std::nullopt;
    return;
  }

  GetSharesheetCallback().Run(
      web_contents(), current_share_->file_paths, current_share_->content_types,
      current_share_->file_sizes, current_share_->text, current_share_->title,
      base::BindOnce(&SharesheetClient::OnShowSharesheet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetClient::OnShowSharesheet(sharesheet::SharesheetResult result) {
  if (!current_share_.has_value())
    return;

  std::move(current_share_->callback).Run(SharesheetResultToShareError(result));
  ScheduleSharedFileDirectoryDeletion(
      std::move(current_share_->file_paths),
      PrepareDirectoryTask::kSharedFileLifetime);
  current_share_ = std::nullopt;
}

// static
void SharesheetClient::ShowSharesheet(
    content::WebContents* web_contents,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& content_types,
    const std::vector<uint64_t>& file_sizes,
    const std::string& text,
    const std::string& title,
    DeliveredCallback delivered_callback) {
  DCHECK_EQ(file_paths.size(), content_types.size());
  DCHECK_EQ(file_paths.size(), file_sizes.size());

  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);

  apps::IntentPtr intent =
      file_paths.empty() ? apps_util::MakeShareIntent(text, title)
                         : apps_util::CreateShareIntentFromFiles(
                               profile, file_paths, content_types, text, title);
  if (!intent->files.empty() && intent->files.size() == file_paths.size()) {
    for (size_t index = 0; index < file_paths.size(); ++index) {
      (intent->files)[index]->mime_type = content_types[index];
      (intent->files)[index]->file_size = file_sizes[index];
    }
  }
  DCHECK(intent->share_text.has_value() || !intent->files.empty());

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);
  sharesheet_service->ShowBubble(web_contents, std::move(intent),
                                 sharesheet::LaunchSource::kWebShare,
                                 std::move(delivered_callback));
}

SharesheetClient::SharesheetCallback&
SharesheetClient::GetSharesheetCallback() {
  static base::NoDestructor<SharesheetCallback> callback(
      base::BindRepeating(&SharesheetClient::ShowSharesheet));

  return *callback;
}

void SharesheetClient::WebContentsDestroyed() {
  current_share_ = std::nullopt;
}

}  // namespace webshare
