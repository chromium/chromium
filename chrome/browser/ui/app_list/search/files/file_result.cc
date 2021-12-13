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
#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

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

}  // namespace

FileResult::FileResult(const std::string& schema,
                       const base::FilePath& filepath,
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

  // Set the details to the display name of the Files app.
  std::u16string sanitized_name = base::CollapseWhitespace(
      l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME), true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  SetDetails(sanitized_name);

  // Launcher search results UI is light by default, so use icons for light
  // background if dark/light mode feature is not enabled.
  const bool dark_background = ash::features::IsDarkLightModeEnabled() &&
                               ash::ColorProvider::Get()->IsDarkModeEnabled();
  if (display_type == DisplayType::kChip) {
    SetChipIcon(ash::GetChipIconForPath(filepath, dark_background));
  } else if (display_type == DisplayType::kContinue) {
    // For Continue Section, if dark/light mode is disabled, we should use the
    // icon and not the chip icon with a dark background as default.
    const gfx::ImageSkia chip_icon =
        ash::features::IsDarkLightModeEnabled()
            ? ash::GetChipIconForPath(filepath, dark_background)
            : ash::GetIconForPath(filepath, /*dark_background=*/true);
    SetChipIcon(chip_icon);
  } else {
    switch (type) {
      case Type::kFile:
        SetIcon(IconInfo(ash::GetIconForPath(filepath, dark_background)));
        break;
      case Type::kDirectory:
        SetIcon(IconInfo(ash::GetIconFromType("folder", dark_background)));
        break;
      case Type::kSharedDirectory:
        SetIcon(IconInfo(ash::GetIconFromType("shared", dark_background)));
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
  const gfx::Size size =
      ash::SharedAppListConfig::instance().search_list_thumbnail_size();
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

  const int dimension =
      ash::SharedAppListConfig::instance().search_list_thumbnail_dimension();
  const auto image = gfx::ImageSkia::CreateFromBitmap(*bitmap, 1.0f);

  SetIcon(ChromeSearchResult::IconInfo(image, dimension,
                                       ash::SearchResultIconShape::kCircle));
}

::std::ostream& operator<<(::std::ostream& os, const FileResult& result) {
  return os << "{" << result.title() << ", " << result.relevance() << "}";
}

}  // namespace app_list
