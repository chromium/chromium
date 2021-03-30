// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_result.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/file_icon_util.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "ui/base/l10n/l10n_util.h"

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

}  // namespace

double CalculateFilenameRelevance(const base::Optional<TokenizedString>& query,
                                  const base::FilePath& path) {
  const TokenizedString title(
      base::UTF8ToUTF16(StripHostedFileExtensions(path.BaseName().value())),
      TokenizedString::Mode::kWords);

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

FileResult::FileResult(const std::string& schema,
                       const base::FilePath& filepath,
                       ResultType result_type,
                       DisplayType display_type,
                       float relevance,
                       Profile* profile)
    : filepath_(filepath), profile_(profile) {
  DCHECK(profile);
  set_id(schema + filepath.value());
  set_relevance(relevance);

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

  SetDisplayType(display_type);
  switch (display_type) {
    case DisplayType::kChip:
      SetChipIcon(ash::GetChipIconForPath(
          filepath, ash::AppListColorProvider::Get()->GetPrimaryIconColor(
                        /*default_color*/ gfx::kGoogleGrey700)));
      break;
    case DisplayType::kList:
      SetIcon(ash::GetIconForPath(
          filepath, ash::AppListColorProvider::Get()->GetPrimaryIconColor(
                        /*default_color*/ gfx::kGoogleGrey700)));
      break;
    default:
      NOTREACHED();
  }

  // Set the details to the display name of the Files app.
  std::u16string sanitized_name = base::CollapseWhitespace(
      l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME), true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  SetDetails(sanitized_name);
  SetTitle(base::UTF8ToUTF16(
      StripHostedFileExtensions(filepath.BaseName().value())));
}

FileResult::~FileResult() = default;

void FileResult::Open(int event_flags) {
  platform_util::OpenItem(profile_, filepath_,
                          platform_util::OpenItemType::OPEN_FILE,
                          platform_util::OpenOperationCallback());
}

::std::ostream& operator<<(::std::ostream& os, const FileResult& result) {
  return os << "{" << result.title() << ", " << result.relevance() << "}";
}

}  // namespace app_list
