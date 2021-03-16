// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_file_provider.h"

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

constexpr char kDriveFileSchema[] = "drive_file://";
constexpr int kMaxResults = 10;

}  // namespace

DriveFileProvider::DriveFileProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(drive_service_);
}

DriveFileProvider::~DriveFileProvider() = default;

ash::AppListSearchResultType DriveFileProvider::ResultType() {
  return ash::AppListSearchResultType::kDriveFile;
}

void DriveFileProvider::Start(const std::u16string& query) {
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
      base::BindOnce(&DriveFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void DriveFileProvider::SetSearchResults(drive::FileError error,
                                         std::vector<base::FilePath> paths) {
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

std::unique_ptr<FileResult> DriveFileProvider::MakeResult(
    const base::FilePath& path) {
  // Reparent the file path into the user's DriveFS mount.
  DCHECK(!path.IsAbsolute());
  const base::FilePath& reparented_path =
      drive_service_->GetMountPointPath().Append(path.value());

  const double relevance =
      CalculateFilenameRelevance(last_tokenized_query_, path);

  return std::make_unique<FileResult>(kDriveFileSchema, reparented_path,
                                      ash::AppListSearchResultType::kDriveFile,
                                      ash::SearchResultDisplayType::kList,
                                      relevance, profile_);
}

}  // namespace app_list
