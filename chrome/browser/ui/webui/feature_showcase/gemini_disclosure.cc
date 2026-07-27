// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_disclosure.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class GeminiDisclosureProvider {
 public:
  virtual ~GeminiDisclosureProvider() = default;
  virtual GeminiDisclosure GetDisclosure(bool is_managed) const = 0;
};

class RowGeminiDisclosureProvider : public GeminiDisclosureProvider {
 public:
  GeminiDisclosure GetDisclosure(bool is_managed) const override {
    std::u16string disclosure_2;
    if (is_managed) {
      disclosure_2 = l10n_util::GetStringUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ENTERPRISE_ROW);
    } else {
      disclosure_2 = l10n_util::GetStringFUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ROW,
          u"https://myactivity.google.com/product/gemini",
          u"https://support.google.com/gemini/?p=activity_settings");
    }

    return {
        .first_paragraph = l10n_util::GetStringUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ROW),
        .second_paragraph = disclosure_2,
        .third_paragraph = l10n_util::GetStringFUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_3_ROW,
            u"https://policies.google.com/terms",
            u"https://support.google.com/gemini/answer/13594961"),
    };
  }
};

class UsGeminiDisclosureProvider : public GeminiDisclosureProvider {
 public:
  GeminiDisclosure GetDisclosure(bool is_managed) const override {
    std::u16string disclosure_2;
    if (is_managed) {
      disclosure_2 = l10n_util::GetStringUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ENTERPRISE_US);
    } else {
      disclosure_2 = l10n_util::GetStringFUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_US,
          u"https://myactivity.google.com/product/gemini",
          u"https://support.google.com/gemini/?p=activity_settings");
    }

    return {
        .first_paragraph = l10n_util::GetStringUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ROW),
        .second_paragraph = disclosure_2,
        .third_paragraph = l10n_util::GetStringFUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_3_ROW,
            u"https://policies.google.com/terms",
            u"https://support.google.com/gemini/answer/13594961"),
    };
  }
};

class KrGeminiDisclosureProvider : public GeminiDisclosureProvider {
 public:
  GeminiDisclosure GetDisclosure(bool is_managed) const override {
    std::u16string disclosure_2;
    if (is_managed) {
      disclosure_2 = l10n_util::GetStringUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ENTERPRISE_ROW);
    } else {
      disclosure_2 = l10n_util::GetStringFUTF16(
          IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_2_ROW,
          u"https://myactivity.google.com/product/gemini",
          u"https://support.google.com/gemini/?p=activity_settings");
    }

    return {
        .first_paragraph = l10n_util::GetStringUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_1_ROW),
        .second_paragraph = disclosure_2,
        .third_paragraph = l10n_util::GetStringFUTF16(
            IDS_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_3_KR,
            u"https://policies.google.com/terms",
            u"https://www.google.com/intl/ko/policies/terms/location",
            u"https://support.google.com/gemini?p=privacy_notice"),
    };
  }
};

std::unique_ptr<GeminiDisclosureProvider> CreateGeminiDisclosureProvider(
    std::string_view country_code) {
  if (country_code == "us") {
    return std::make_unique<UsGeminiDisclosureProvider>();
  }
  if (country_code == "kr") {
    return std::make_unique<KrGeminiDisclosureProvider>();
  }
  return std::make_unique<RowGeminiDisclosureProvider>();
}

}  // namespace

GeminiDisclosure GetGeminiDisclosure(std::string_view country_code,
                                     bool is_managed) {
  return CreateGeminiDisclosureProvider(base::ToLowerASCII(country_code))
      ->GetDisclosure(is_managed);
}
