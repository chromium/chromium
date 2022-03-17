// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_result.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/files/justifications.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

// The maximum penalty applied to a relevance by PenalizeRelevanceByAccessTime,
// which will multiply the relevance by a number in [`kMaxPenalty`, 1].
constexpr double kMaxPenalty = 0.6;

// The steepness of the penalty curve of PenalizeRelevanceByAccessTime. Larger
// values make the penalty increase faster as the last access time of the file
// increases. A value of 0.0029 results in a penalty multiplier of ~0.63 for a 1
// month old file.
constexpr double kPenaltyCoeff = 0.0029;

constexpr int64_t kMillisPerDay = 1000 * 60 * 60 * 24;

std::string StripHostedFileExtensions(const std::string& filename) {
  static const base::NoDestructor<std::vector<std::string>> hosted_extensions(
      {".GDOC", ".GSHEET", ".GSLIDES", ".GDRAW", ".GTABLE", ".GLINK", ".GFORM",
       ".GMAPS", ".GSITE"});

  for (const auto& extension : *hosted_extensions) {
    if (EndsWith(filename, extension, base::CompareCase::INSENSITIVE_ASCII)) {
      return filename.substr(0, filename.size() - extension.size());
    }
  }

  return filename;
}

void LogRelevance(ChromeSearchResult::ResultType result_type,
                  const double relevance) {
  // Relevance scores are between 0 and 1, so we scale to 0 to 100 for logging.
  DCHECK((relevance >= 0) && (relevance <= 1));
  const int scaled_relevance = floor(100 * relevance);
  switch (result_type) {
    case FileResult::ResultType::kFileSearch:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.FileSearchProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kDriveSearch:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.DriveSearchProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kZeroStateFile:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.ZeroStateFileProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kZeroStateDrive:
      UMA_HISTOGRAM_EXACT_LINEAR(
          "Apps.AppList.ZeroStateDriveProvider.Relevance", scaled_relevance,
          /*exclusive_max=*/101);
      break;
    default:
      NOTREACHED();
  }
}

absl::optional<base::File::Info> GetFileInfo(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return absl::nullopt;
  return info;
}

}  // namespace

FileResult::FileResult(const std::string& schema,
                       const base::FilePath& filepath,
                       const absl::optional<std::u16string>& details,
                       ResultType result_type,
                       DisplayType display_type,
                       float relevance,
                       const std::u16string& query,
                       Type type,
                       Profile* profile)
    : filepath_(filepath), type_(type), profile_(profile) {
  DCHECK(profile);
  set_id(schema + filepath.value());
  SetCategory(Category::kFiles);
  SetDisplayType(display_type);

  set_relevance(relevance);
  if (display_type == DisplayType::kList) {
    // Chip and list results overlap, and only list results are fully launched.
    // So we only log metrics for list results.
    LogRelevance(result_type, relevance);
  }

  SetResultType(result_type);
  switch (result_type) {
    case ResultType::kDriveChip:
    case ResultType::kZeroStateDrive:
      SetMetricsType(ash::ZERO_STATE_DRIVE);
      break;
    case ResultType::kFileChip:
    case ResultType::kZeroStateFile:
      SetMetricsType(ash::ZERO_STATE_FILE);
      break;
    case ResultType::kFileSearch:
      SetMetricsType(ash::FILE_SEARCH);
      break;
    case ResultType::kDriveSearch:
      SetMetricsType(ash::DRIVE_SEARCH);
      break;
    default:
      NOTREACHED();
  }

  SetTitle(base::UTF8ToUTF16(
      StripHostedFileExtensions(filepath.BaseName().value())));
  SetTitleTags(CalculateTags(query, title()));

  if (details)
    SetDetails(details.value());

  // Launcher search results UI is light by default, so use icons for light
  // background if dark/light mode feature is not enabled. Productivity launcher
  // has dark background by default, so use icons for dark background in that
  // case.
  const bool dark_background =
      ash::features::IsDarkLightModeEnabled()
          ? ash::ColorProvider::Get()->IsDarkModeEnabled()
          : ash::features::IsProductivityLauncherEnabled();
  if (display_type == DisplayType::kChip) {
    SetChipIcon(chromeos::GetChipIconForPath(filepath, dark_background));
  } else if (display_type == DisplayType::kContinue) {
    // For Continue Section, if dark/light mode is disabled, we should use the
    // icon and not the chip icon with a dark background as default.
    const gfx::ImageSkia chip_icon =
        ash::features::IsDarkLightModeEnabled()
            ? chromeos::GetChipIconForPath(filepath, dark_background)
            : chromeos::GetIconForPath(filepath, /*dark_background=*/true);
    SetChipIcon(chip_icon);
  } else {
    switch (type) {
      case Type::kFile:
        SetIcon(IconInfo(chromeos::GetIconForPath(filepath, dark_background),
                         kSystemIconDimension));
        break;
      case Type::kDirectory:
        SetIcon(IconInfo(chromeos::GetIconFromType("folder", dark_background),
                         kSystemIconDimension));
        break;
      case Type::kSharedDirectory:
        SetIcon(IconInfo(chromeos::GetIconFromType("shared", dark_background),
                         kSystemIconDimension));
        break;
    }
  }
}

FileResult::~FileResult() = default;

void FileResult::Open(int event_flags) {
  switch (type_) {
    case Type::kFile:
      platform_util::OpenItem(profile_, filepath_,
                              platform_util::OpenItemType::OPEN_FILE,
                              platform_util::OpenOperationCallback());
      break;
    case Type::kDirectory:
    case Type::kSharedDirectory:
      platform_util::OpenItem(profile_, filepath_,
                              platform_util::OpenItemType::OPEN_FOLDER,
                              platform_util::OpenOperationCallback());
      break;
  }
}

absl::optional<std::string> FileResult::DriveId() const {
  return drive_id_;
}

// static
double FileResult::CalculateRelevance(
    const absl::optional<TokenizedString>& query,
    const base::FilePath& filepath) {
  const std::u16string raw_title =
      base::UTF8ToUTF16(StripHostedFileExtensions(filepath.BaseName().value()));
  const TokenizedString title(raw_title, TokenizedString::Mode::kWords);

  const bool use_default_relevance =
      !query || query.value().text().empty() || title.text().empty();
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.FileResult.DefaultRelevanceUsed",
                        use_default_relevance);
  if (use_default_relevance) {
    static constexpr double kDefaultRelevance = 0.5;
    return kDefaultRelevance;
  }

  TokenizedStringMatch match;
  match.Calculate(query.value(), title);
  return match.relevance();
}

void FileResult::RequestThumbnail(ash::ThumbnailLoader* thumbnail_loader) {
  // Thumbnails are only available for list results.
  DCHECK_EQ(display_type(), DisplayType::kList);

  // Request a thumbnail for all file types. For unsupported types, this will
  // just call OnThumbnailLoaded with an error.
  const gfx::Size size = gfx::Size(kThumbnailDimension, kThumbnailDimension);
  thumbnail_loader->Load({filepath_, size},
                         base::BindOnce(&FileResult::OnThumbnailLoaded,
                                        weak_factory_.GetWeakPtr()));
}

void FileResult::OnThumbnailLoaded(const SkBitmap* bitmap,
                                   base::File::Error error) {
  if (!bitmap) {
    DCHECK_NE(error, base::File::Error::FILE_OK);
    base::UmaHistogramExactLinear(
        "Apps.AppList.FileResult.ThumbnailLoadedError", -error,
        -base::File::FILE_ERROR_MAX);
    return;
  }

  DCHECK_EQ(error, base::File::Error::FILE_OK);

  const int dimension = kThumbnailDimension;
  const auto image = gfx::ImageSkia::CreateFromBitmap(*bitmap, 1.0f);

  SetIcon(ChromeSearchResult::IconInfo(image, dimension,
                                       ash::SearchResultIconShape::kCircle));
}

void FileResult::PenalizeRelevanceByAccessTime() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFileInfo, filepath_),
      base::BindOnce(&FileResult::OnFileInfoReturned,
                     weak_factory_.GetWeakPtr()));
}

void FileResult::OnFileInfoReturned(
    const absl::optional<base::File::Info>& info) {
  // Do not penalize relevance if we can't stat the file.
  if (!info) {
    return;
  }

  // Apply a gaussian penalty based on the time delta. `time_delta` is converted
  // into millisecond fractions of a day for numerical stability.
  double time_delta =
      static_cast<double>(
          (base::Time::Now() - info->last_accessed).InMilliseconds()) /
      kMillisPerDay;
  double penalty =
      kMaxPenalty +
      (1.0 - kMaxPenalty) * std::exp(-kPenaltyCoeff * time_delta * time_delta);
  DCHECK((penalty > 0.0) && (penalty <= 1.0));
  set_relevance(relevance() * penalty);
}

void FileResult::SetDetailsToJustificationString() {
  GetJustificationStringAsync(
      filepath_, base::BindOnce(&FileResult::OnJustificationStringReturned,
                                weak_factory_.GetWeakPtr()));
}

void FileResult::OnJustificationStringReturned(
    absl::optional<std::u16string> justification) {
  if (justification)
    SetDetails(justification.value());
}

::std::ostream& operator<<(::std::ostream& os, const FileResult& result) {
  return os << "{" << result.title() << ", " << result.relevance() << "}";
}

}  // namespace app_list
