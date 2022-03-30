// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_search_provider.h"

#include <cmath>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/string_util.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

constexpr char kDriveSearchSchema[] = "drive_search://";
constexpr int kMaxResults = 50;
constexpr size_t kMinQuerySizeForSharedFiles = 5u;

// Outcome of a call to DriveSearchProvider::Start. These values persist
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Status {
  kOk = 0,
  kDriveUnavailable = 1,
  kFileError = 2,
  kMaxValue = kFileError,
};

void LogStatus(Status status) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.DriveSearchProvider.Status", status);
}

}  // namespace

DriveSearchProvider::DriveSearchProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(drive_service_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DriveSearchProvider::~DriveSearchProvider() = default;

ash::AppListSearchResultType DriveSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kDriveSearch;
}

void DriveSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  // Clear results and cancel any outgoing requests.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  if (!drive_service_ || !drive_service_->is_enabled()) {
    LogStatus(Status::kDriveUnavailable);
    return;
  }

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  drive_service_->SearchDriveByFileName(
      base::UTF16ToUTF8(query), kMaxResults,
      drivefs::mojom::QueryParameters::SortField::kLastModified,
      drivefs::mojom::QueryParameters::SortDirection::kDescending,
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
      base::BindOnce(&DriveSearchProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void DriveSearchProvider::SetSearchResults(
    drive::FileError error,
    std::vector<drivefs::mojom::QueryItemPtr> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FileError::FILE_ERROR_OK) {
    LogStatus(Status::kFileError);
    return;
  }

  // Filter out shared files if the query length is below a threshold.
  if (last_query_.size() < kMinQuerySizeForSharedFiles) {
    std::vector<drivefs::mojom::QueryItemPtr> filtered_items;
    for (auto& item : items) {
      if (!item->metadata->shared)
        filtered_items.push_back(std::move(item));
    }
    items = std::move(filtered_items);
  }

  SearchProvider::Results results;
  for (const auto& item : items) {
    double relevance =
        FileResult::CalculateRelevance(last_tokenized_query_, item->path);

    std::unique_ptr<FileResult> result;
    GURL url(item->metadata->alternate_url);
    if (item->metadata->type ==
        drivefs::mojom::FileMetadata::Type::kDirectory) {
      const auto type = item->metadata->shared
                            ? FileResult::Type::kSharedDirectory
                            : FileResult::Type::kDirectory;
      result = MakeResult(item->path, relevance, type, GetDriveId(url));
    } else {
      result = MakeResult(item->path, relevance, FileResult::Type::kFile,
                          GetDriveId(url));
    }
    result->PenalizeRelevanceByAccessTime();
    results.push_back(std::move(result));
  }

  SwapResults(&results);
  LogStatus(Status::kOk);
  UMA_HISTOGRAM_TIMES("Apps.AppList.DriveSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> DriveSearchProvider::MakeResult(
    const base::FilePath& path,
    double relevance,
    FileResult::Type type,
    const absl::optional<std::string>& drive_id) {
  // Strip leading separators so that the path can be reparented.
  // TODO(crbug.com/1154513): Remove this step once the drive backend returns
  // results in relative path format.
  DCHECK(!path.value().empty());
  const base::FilePath relative_path =
      !path.value().empty() && base::FilePath::IsSeparator(path.value()[0])
          ? base::FilePath(path.value().substr(1))
          : path;

  // Reparent the file path into the user's DriveFS mount.
  DCHECK(!relative_path.IsAbsolute());
  const base::FilePath& reparented_path =
      drive_service_->GetMountPointPath().Append(relative_path.value());

  // Add "Google Drive" as details.
  std::u16string details =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);

  auto result =
      std::make_unique<FileResult>(kDriveSearchSchema, reparented_path, details,
                                   ash::AppListSearchResultType::kDriveSearch,
                                   ash::SearchResultDisplayType::kList,
                                   relevance, last_query_, type, profile_);
  result->set_drive_id(drive_id);
  return result;
}

}  // namespace app_list
