// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_info.h"

#include <windows.h>

#include <fileapi.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/file_version_info.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {

// Document the assumptions made on the ProcessType enum in order to convert
// them to bits.
static_assert(content::PROCESS_TYPE_UNKNOWN == 1,
              "assumes unknown process type has value 1");
static_assert(content::PROCESS_TYPE_BROWSER == 2,
              "assumes browser process type has value 2");
constexpr uint32_t kFirstValidProcessType = content::PROCESS_TYPE_BROWSER;

// Using the module path, populates |inspection_result| with information
// available via the file on disk. For example, this includes the description
// and the certificate information.
void PopulateModuleInfoData(const base::FilePath& module_path,
                            ModuleInspectionResult* inspection_result) {
  inspection_result->location = module_path.AsUTF16Unsafe();

  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(module_path));
  if (file_version_info) {
    inspection_result->product_name = file_version_info->product_name();
    inspection_result->description = file_version_info->file_description();
    inspection_result->version = file_version_info->product_version();
  }

  GetCertificateInfo(module_path, &(inspection_result->certificate_info));
}

// Returns the long path name given a short path name. A short path name is a
// path that follows the 8.3 convention and has ~x in it. If the path is already
// a long path name, the function returns the current path without modification.
bool ConvertToLongPath(const std::u16string& short_path,
                       std::u16string* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value =
      ::GetLongPathName(base::as_wcstr(short_path), long_path_buf, MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = base::AsString16(std::wstring(long_path_buf));
    return true;
  }

  return false;
}

}  // namespace

// ModuleInfoKey ---------------------------------------------------------------

ModuleInfoKey::ModuleInfoKey(const base::FilePath& module_path,
                             uint32_t module_size,
                             uint32_t module_time_date_stamp)
    : module_path(module_path),
      module_size(module_size),
      module_time_date_stamp(module_time_date_stamp) {}

bool ModuleInfoKey::operator<(const ModuleInfoKey& mik) const {
  // The key consists of the triplet of
  // (module_path, module_size, module_time_date_stamp).
  // Use the std::tuple lexicographic comparison operator.
  return std::tie(module_path, module_size, module_time_date_stamp) <
         std::tie(mik.module_path, mik.module_size, mik.module_time_date_stamp);
}

bool ModuleInfoKey::operator==(const ModuleInfoKey& mik) const {
  return std::tie(module_path, module_size, module_time_date_stamp) ==
         std::tie(mik.module_path, mik.module_size, mik.module_time_date_stamp);
}

// ModuleInspectionResult ------------------------------------------------------

ModuleInspectionResult::ModuleInspectionResult() = default;

ModuleInspectionResult::ModuleInspectionResult(
    const ModuleInspectionResult& other) = default;
ModuleInspectionResult::ModuleInspectionResult(ModuleInspectionResult&& other) =
    default;

ModuleInspectionResult& ModuleInspectionResult::operator=(
    const ModuleInspectionResult& other) = default;
ModuleInspectionResult& ModuleInspectionResult::operator=(
    ModuleInspectionResult&& other) = default;

ModuleInspectionResult::~ModuleInspectionResult() = default;

// ModuleInfoData --------------------------------------------------------------

ModuleInfoData::ModuleInfoData() : process_types(0), module_properties(0) {}

ModuleInfoData::~ModuleInfoData() = default;

ModuleInfoData::ModuleInfoData(ModuleInfoData&& module_data) noexcept = default;

// -----------------------------------------------------------------------------

ModuleInspectionResult InspectModule(const base::FilePath& module_path) {
  ModuleInspectionResult inspection_result;

  PopulateModuleInfoData(module_path, &inspection_result);
  internal::NormalizeInspectionResult(&inspection_result);

  return inspection_result;
}

// Returns the time stamp to be used in the inspection results cache.
// Represents the number of hours between |time| and the Windows epoch
// (1601-01-01 00:00:00 UTC).
uint32_t CalculateTimeStamp(base::Time time) {
  const auto delta = time.ToDeltaSinceWindowsEpoch();
  return delta.is_negative() ? 0 : static_cast<uint32_t>(delta.InHours());
}

std::string GenerateCodeId(const ModuleInfoKey& module_key) {
  return base::StringPrintf("%08X%x", module_key.module_time_date_stamp,
                            module_key.module_size);
}

uint32_t ProcessTypeToBit(content::ProcessType process_type) {
  uint32_t bit_index =
      static_cast<uint32_t>(process_type) - kFirstValidProcessType;
  DCHECK_GE(31u, bit_index);
  uint32_t bit = (1 << bit_index);
  return bit;
}

content::ProcessType BitIndexToProcessType(uint32_t bit_index) {
  DCHECK_GE(31u, bit_index);
  return static_cast<content::ProcessType>(bit_index + kFirstValidProcessType);
}

bool IsBlockingEnabledInProcessTypes(uint32_t process_types) {
  uint64_t process_types_mask =
      ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
      ProcessTypeToBit(content::PROCESS_TYPE_RENDERER);

  return (process_types & process_types_mask) != 0;
}

namespace internal {

void NormalizeInspectionResult(ModuleInspectionResult* inspection_result) {
  std::u16string path = inspection_result->location;
  if (!ConvertToLongPath(path, &inspection_result->location))
    inspection_result->location = path;

  inspection_result->location =
      base::i18n::ToLower(inspection_result->location);

  // Location contains the filename, so the last slash is where the path
  // ends.
  size_t last_slash = inspection_result->location.find_last_of('\\');
  if (last_slash != std::u16string::npos) {
    inspection_result->basename =
        inspection_result->location.substr(last_slash + 1);
    inspection_result->location =
        inspection_result->location.substr(0, last_slash + 1);
  } else {
    inspection_result->basename = inspection_result->location;
    inspection_result->location.clear();
  }

  // Some version strings use ", " instead ".". Convert those.
  base::ReplaceSubstringsAfterOffset(&inspection_result->version, 0, u", ",
                                     u".");

  // Some version strings have things like (win7_rtm.090713-1255) appended
  // to them. Remove that.
  size_t first_space = inspection_result->version.find_first_of(u" ");
  if (first_space != std::u16string::npos)
    inspection_result->version =
        inspection_result->version.substr(0, first_space);
}

}  // namespace internal
