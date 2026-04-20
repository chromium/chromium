// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/third_party_metrics_recorder.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "components/crash/core/common/crash_key.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#endif

ThirdPartyMetricsRecorder::ThirdPartyMetricsRecorder() {
  current_value_.reserve(kCrashKeySize);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Emit the result of applying the NtMapViewOfSection hook in chrome_elf.dll.
  base::UmaHistogramSparse("ChromeElf.ApplyHookResult", GetApplyHookResult());
#endif
}

ThirdPartyMetricsRecorder::~ThirdPartyMetricsRecorder() = default;

void ThirdPartyMetricsRecorder::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  const CertificateInfo& certificate_info =
      module_data.inspection_result->certificate_info;
  if (certificate_info.type == CertificateInfo::Type::NO_CERTIFICATE) {
    // Put unsigned modules into the crash keys.
    if (module_data.module_properties & ModuleInfoData::kPropertyLoadedModule) {
      AddUnsignedModuleToCrashkeys(
          base::AsWString(module_data.inspection_result->basename));
    }
  }
}

void ThirdPartyMetricsRecorder::OnModuleDatabaseIdle() {}

void ThirdPartyMetricsRecorder::AddUnsignedModuleToCrashkeys(
    const std::wstring& module_basename) {
  using UnsignedModulesKey = crash_reporter::CrashKeyString<kCrashKeySize>;
  static std::array<UnsignedModulesKey, 5> unsigned_modules_keys{{
      {"unsigned-modules-1", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-2", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-3", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-4", UnsignedModulesKey::Tag::kArray},
      {"unsigned-modules-5", UnsignedModulesKey::Tag::kArray},
  }};

  if (current_key_index_ >= std::size(unsigned_modules_keys))
    return;

  std::string module = base::WideToUTF8(module_basename);

  // Truncate the basename if it doesn't fit in one crash key.
  size_t module_length = std::min(module.length(), kCrashKeySize);

  // Check if the module fits in the current string or if a new string is
  // needed.
  size_t length_remaining = kCrashKeySize;
  if (!current_value_.empty())
    length_remaining -= current_value_.length() + 1;

  if (module_length > length_remaining) {
    current_value_.clear();

    if (++current_key_index_ >= std::size(unsigned_modules_keys))
      return;
  }

  // Append the module to the current string. Separate with a comma if needed.
  if (!current_value_.empty())
    current_value_.append(",");
  current_value_.append(module, 0, module_length);

  unsigned_modules_keys[current_key_index_].Set(current_value_);
}
