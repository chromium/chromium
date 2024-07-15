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
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "components/crash/core/common/crash_key.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#endif

namespace {

// Returns true if the module is signed by Google.
bool IsGoogleModule(std::u16string_view subject) {
  static constexpr std::u16string_view kGoogleLlc(u"Google LLC");
  static constexpr std::u16string_view kGoogleInc(u"Google Inc");
  return subject == kGoogleLlc || subject == kGoogleInc;
}

}  // namespace

ThirdPartyMetricsRecorder::ThirdPartyMetricsRecorder() {
  current_value_.reserve(kCrashKeySize);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // It is safe to use base::Unretained() since the timer is a member variable
  // of this class.
  heartbeat_metrics_timer_.Start(
      FROM_HERE, base::Minutes(5),
      base::BindRepeating(&ThirdPartyMetricsRecorder::RecordHeartbeatMetrics,
                          base::Unretained(this)));

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
  module_count_++;
  if (certificate_info.type != CertificateInfo::Type::NO_CERTIFICATE) {
    ++signed_module_count_;

    if (certificate_info.type == CertificateInfo::Type::CERTIFICATE_IN_CATALOG)
      ++catalog_module_count_;

    std::u16string_view certificate_subject = certificate_info.subject;
    if (IsMicrosoftModule(certificate_subject)) {
      ++microsoft_module_count_;
    } else if (IsGoogleModule(certificate_subject)) {
      // No need to count these explicitly.
    } else {
      // Count modules that are neither signed by Google nor Microsoft.
      // These are considered "third party" modules.
      if (module_data.module_properties &
          ModuleInfoData::kPropertyLoadedModule) {
        ++loaded_third_party_module_count_;
      } else {
        ++not_loaded_third_party_module_count_;
      }
    }
  } else {
    ++unsigned_module_count_;

    // Put unsigned modules into the crash keys.
    if (module_data.module_properties & ModuleInfoData::kPropertyLoadedModule) {
      AddUnsignedModuleToCrashkeys(
          base::AsWString(module_data.inspection_result->basename));
    }
  }

  if (module_data.module_properties & ModuleInfoData::kPropertyShellExtension)
    shell_extensions_count_++;
}

void ThirdPartyMetricsRecorder::OnModuleDatabaseIdle() {
  if (metrics_emitted_)
    return;
  metrics_emitted_ = true;

  // Report back some metrics regarding third party modules and certificates.
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Loaded",
                                 loaded_third_party_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.NotLoaded",
                                 not_loaded_third_party_module_count_, 1, 500,
                                 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed",
                                 signed_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed.Microsoft",
                                 microsoft_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Signed.Catalog",
                                 catalog_module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Total",
                                 module_count_, 1, 500, 50);
  base::UmaHistogramCustomCounts("ThirdPartyModules.Modules.Unsigned",
                                 unsigned_module_count_, 1, 500, 50);

  base::UmaHistogramCounts100("ThirdPartyModules.ShellExtensionsCount3",
                              shell_extensions_count_);
}

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ThirdPartyMetricsRecorder::RecordHeartbeatMetrics() {
  UMA_HISTOGRAM_COUNTS_1M(
      "ThirdPartyModules.Heartbeat.UniqueBlockedModulesCount",
      GetUniqueBlockedModulesCount());

  if (record_blocked_modules_count_) {
    uint32_t blocked_modules_count = GetBlockedModulesCount();
    UMA_HISTOGRAM_COUNTS_1M("ThirdPartyModules.Heartbeat.BlockedModulesCount",
                            blocked_modules_count);

    // Stop recording when |blocked_modules_count| gets too high. This is to
    // avoid dealing with the possible integer overflow that would result in
    // emitting wrong values. The exact cutoff point is not important but it
    // must be higher than the max value for the histogram (1M in this case).
    // It's ok to continue logging the count of unique blocked modules because
    // there's no expectation that this count can reach a high value.
    if (blocked_modules_count > std::numeric_limits<uint32_t>::max() / 2)
      record_blocked_modules_count_ = false;
  }

  UMA_HISTOGRAM_BOOLEAN(
      "ThirdPartyModules.Heartbeat.PrintingWorkaround.BlockingEnabled",
      hook_enabled_);
}
#endif
