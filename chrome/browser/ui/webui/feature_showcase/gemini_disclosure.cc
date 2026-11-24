// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_disclosure.h"

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::u16string GetFirstParagraph(std::string_view country_code,
                                 bool is_managed) {
  const bool is_us = (country_code == "us");
  if (is_managed) {
    static constexpr char16_t kGoogleWorkspaceDataNoticeUrl[] =
        u"https://knowledge.workspace.google.com/admin/generative-ai/"
        u"generative-ai-in-google-workspace-privacy-hub";
    const int message_id =
        is_us ? IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ENTERPRISE_US
              : IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ENTERPRISE_ROW;
    return l10n_util::GetStringFUTF16(message_id,
                                      kGoogleWorkspaceDataNoticeUrl);
  }

  static constexpr char16_t kGeminiAppsActivityUrl[] =
      u"https://myactivity.google.com/product/gemini";
  static constexpr char16_t kGeminiLocationInfoUrl[] =
      u"https://support.google.com/gemini?p=gemini_mobile_app_location_info";
  static constexpr char16_t kGeminiChoicesUrl[] =
      u"https://support.google.com/gemini?p=your_choices";
  const int message_id = is_us ? IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_US
                               : IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ROW;
  return l10n_util::GetStringFUTF16(message_id, kGeminiAppsActivityUrl,
                                    kGeminiLocationInfoUrl, kGeminiChoicesUrl);
}

std::u16string GetSecondParagraph(std::string_view country_code) {
  static constexpr char16_t kGoogleTermsUrl[] =
      u"https://policies.google.com/terms";
  static constexpr char16_t kGeminiPrivacyNoticeUrl[] =
      u"https://support.google.com/gemini/answer/13594961";
  if (country_code == "kr") {
    static constexpr char16_t kKoreanLocationTermsUrl[] =
        u"https://www.google.com/intl/ko/policies/terms/location";
    return l10n_util::GetStringFUTF16(
        IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_KR, kGoogleTermsUrl,
        kKoreanLocationTermsUrl, kGeminiPrivacyNoticeUrl);
  }
  return l10n_util::GetStringFUTF16(
      IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ROW, kGoogleTermsUrl,
      kGeminiPrivacyNoticeUrl);
}

}  // namespace

GeminiDisclosure GetGeminiDisclosure(std::string_view country_code,
                                     bool is_managed) {
  const std::string normalized_country_code = base::ToLowerASCII(country_code);
  return {
      .first_paragraph = GetFirstParagraph(normalized_country_code, is_managed),
      .second_paragraph = GetSecondParagraph(normalized_country_code),
  };
}
