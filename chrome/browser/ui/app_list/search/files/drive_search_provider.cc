// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_search_provider.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

constexpr char kDriveSearchSchema[] = "drive_search://";
constexpr int kMaxResults = 10;

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

ash::AppListSearchResultType DriveSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kDriveSearch;
}

void DriveSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear results and cancel any outgoing requests.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  // This provider does not handle zero-state.
  if (query.empty())
    return;

  if (!drive_service_ || !drive_service_->is_enabled()) {
    // TODO(crbug.com/1154513): Log error histogram.
    return;
  }

  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  // New scores will be assigned for sorting purposes so use the default
  // SortField. The SortDirection does nothing in this case.
  drive_service_->SearchDriveByFileName(
      base::UTF16ToUTF8(query), kMaxResults,
      drivefs::mojom ::QueryParameters::SortField::kNone,
      drivefs::mojom ::QueryParameters::SortDirection::kAscending,
      base::BindOnce(&DriveSearchProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void DriveSearchProvider::SetSearchResults(drive::FileError error,
                                           std::vector<base::FilePath> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FileError::FILE_ERROR_OK) {
    // TODO(crbug.com/1154513): Log error histogram.
    return;
  }

  SearchProvider::Results results;
  for (const auto& path : paths) {
    results.emplace_back(MakeResult(path));
  }

  SwapResults(&results);
  // TODO(crbug.com/1154513): Log success and latency histograms.
}

std::unique_ptr<FileResult> DriveSearchProvider::MakeResult(
    const base::FilePath& path) {
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

  const double relevance =
      CalculateFilenameRelevance(last_tokenized_query_, relative_path);

  return std::make_unique<FileResult>(
      kDriveSearchSchema, reparented_path,
      ash::AppListSearchResultType::kDriveSearch,
      ash::SearchResultDisplayType::kList, relevance, profile_);
}

}  // namespace app_list
