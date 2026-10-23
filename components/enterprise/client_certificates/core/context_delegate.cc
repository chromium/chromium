// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/context_delegate.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {

bool ShouldStripControlCharacter(base_icu::UChar32 code_point) {
  // Strip Unicode control characters (Cc: C0 0x0000..0x001F, C1
  // 0x007F..0x009F), format characters (Cf), and line/paragraph separators
  // (Zl/Zp: 0x2028..0x2029) so they cannot cause CAS to reject the certificate
  // request or spoof the Common Name, while preserving visible punctuation
  // (such as hyphens, dashes, and curly apostrophes).
  return base::IsUnicodeControl(code_point) || code_point == 0x00AD ||
         code_point == 0x061C || code_point == 0x180E ||
         (code_point >= 0x200B && code_point <= 0x200F) ||
         (code_point >= 0x2028 && code_point <= 0x202E) ||
         (code_point >= 0x2060 && code_point <= 0x206F) ||
         code_point == 0xFEFF ||
         (code_point >= 0xFFF9 && code_point <= 0xFFFB) ||
         code_point == 0xE0001 ||
         (code_point >= 0xE0020 && code_point <= 0xE007F);
}

}  // namespace

bool IsCertExpiringSoon(const net::X509Certificate& certificate) {
  return (base::Time::Now() + base::Days(kDaysBeforeExpiration)) >
         certificate.valid_expiry();
}

std::string SanitizeAndTruncateCommonName(std::string_view raw_name) {
  bool control_characters_stripped = false;
  std::u16string u16_name;
  for (size_t i = 0; i < raw_name.size(); ++i) {
    base_icu::UChar32 code_point = 0;
    if (!base::ReadUnicodeCharacter(raw_name, &i, &code_point) ||
        ShouldStripControlCharacter(code_point)) {
      control_characters_stripped = true;
    } else {
      base::WriteUnicodeCharacter(code_point, &u16_name);
    }
  }

  // Convert between UTF-16 and UTF-8 because `base::TrimWhitespace` only
  // supports UTF-16 for Unicode whitespace, while `kMaxCommonNameLength`
  // truncation is measured in UTF-8 bytes via `base::TruncateUTF8ToByteSize`.
  bool whitespace_trimmed = base::TrimWhitespace(u16_name, base::TRIM_ALL,
                                                 &u16_name) != base::TRIM_NONE;

  std::string result = base::UTF16ToUTF8(u16_name);
  bool truncated = result.size() > kMaxCommonNameLength;
  if (truncated) {
    result.resize(
        base::TruncateUTF8ToByteSize(result, kMaxCommonNameLength).size());
    if (base::TrimWhitespace(base::UTF8ToUTF16(result), base::TRIM_TRAILING,
                             &u16_name) != base::TRIM_NONE) {
      whitespace_trimmed = true;
      result = base::UTF16ToUTF8(u16_name);
    }
  }

  if (control_characters_stripped) {
    LogCommonNameSanitization(
        CommonNameSanitizationType::kControlCharactersStripped);
  }
  if (whitespace_trimmed) {
    LogCommonNameSanitization(CommonNameSanitizationType::kWhitespaceTrimmed);
  }
  if (truncated) {
    LogCommonNameSanitization(CommonNameSanitizationType::kTruncated);
  }
  if (result.empty()) {
    LogCommonNameSanitization(CommonNameSanitizationType::kEmptyResult);
  } else if (!control_characters_stripped && !whitespace_trimmed &&
             !truncated) {
    LogCommonNameSanitization(CommonNameSanitizationType::kNone);
  }

  return result;
}

ContextDelegate::ContextDelegate() = default;
ContextDelegate::~ContextDelegate() = default;

void ContextDelegate::SetComputerNameForTesting(
    std::optional<std::string> computer_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  computer_name_for_testing_ = std::move(computer_name);
  cached_computer_name_.reset();
}

std::optional<std::string> ContextDelegate::GetConfiguredComputerName(
    const PrefService* prefs) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS)
  // Computer name is not supported on ChromeOS (where policy::GetDeviceName()
  // returns the hardware serial number rather than a hostname).
  return std::nullopt;
#else
  if (!prefs || !features::IsClientCertCommonNameAttributeEnabled()) {
    return std::nullopt;
  }
  std::string config_pref = GetConfigPolicyPref();
  if (config_pref.empty() || !prefs->FindPreference(config_pref)) {
    return std::nullopt;
  }
  const std::string* common_name_source =
      prefs->GetDict(config_pref).FindString(kCommonNameSourceKey);
  if (!common_name_source ||
      *common_name_source != kCommonNameSourceComputerName) {
    return std::nullopt;
  }
  if (!cached_computer_name_.has_value()) {
    std::string raw_name = computer_name_for_testing_.has_value()
                               ? *computer_name_for_testing_
                               : policy::GetDeviceName();
    std::string sanitized = SanitizeAndTruncateCommonName(raw_name);
    if (sanitized.empty()) {
      return std::nullopt;
    }
    cached_computer_name_ = std::move(sanitized);
  }
  return cached_computer_name_;
#endif
}

bool ContextDelegate::ShouldCertificateBeRenewed(
    const net::X509Certificate& certificate,
    std::string_view expected_common_name) {
  std::string logging_context = GetLoggingContext();
  if (certificate.valid_expiry() <= base::Time::Now()) {
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "[" << logging_context << "] Certificate is expired (expired at "
        << certificate.valid_expiry() << ").";
    LogCertificateValidationResult(logging_context,
                                   CertificateValidationResult::kExpired);
    return true;
  }

  if (IsCertExpiringSoon(certificate)) {
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "[" << logging_context
        << "] Certificate is expiring soon (expires at "
        << certificate.valid_expiry() << ").";
    LogCertificateValidationResult(logging_context,
                                   CertificateValidationResult::kExpiringSoon);
    return true;
  }

  if (expected_common_name.empty()) {
    LOG_POLICY(WARNING, DEVICE_TRUST)
        << "[" << logging_context
        << "] Cannot validate certificate: expected identity is missing.";
    LogCertificateValidationResult(
        logging_context, CertificateValidationResult::kMissingContextIdentity);
    return false;
  }

  const std::string& common_name = certificate.subject().common_name;
  if (common_name.empty()) {
    LOG_POLICY(WARNING, DEVICE_TRUST)
        << "[" << logging_context
        << "] Certificate Subject Common Name is empty.";
    LogCertificateValidationResult(
        logging_context, CertificateValidationResult::kEmptySubjectCommonName);
    return features::IsClientCertCommonNameAttributeEnabled();
  }

  if (common_name == expected_common_name) {
    LogCertificateValidationResult(logging_context,
                                   CertificateValidationResult::kValid);
    return false;
  }

  LOG_POLICY(WARNING, DEVICE_TRUST)
      << "[" << logging_context
      << "] Certificate Subject Common Name does not match expected identity.";
  LogCertificateValidationResult(
      logging_context, CertificateValidationResult::kIdentityMismatch);
  return features::IsClientCertCommonNameAttributeEnabled();
}

std::string ContextDelegate::GetConfigPolicyPref() const {
  return std::string();
}

}  // namespace client_certificates
