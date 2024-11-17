// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/enterprise_interstitial_base.h"

#include "components/enterprise/connectors/core/enterprise_interstitial_util.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_connectors {

void EnterpriseInterstitialBase::PopulateStrings(
    base::Value::Dict& load_time_data) const {
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("primaryButtonText", "");
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  std::u16string custom_message =
      GetUrlFilteringCustomMessage(unsafe_resources());
  if (!custom_message.empty()) {
    load_time_data.Set(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(GetCustomMessagePrimaryParagraphMessageId(),
                                   std::move(custom_message)));
  } else {
    load_time_data.Set(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(
            GetPrimaryParagraphMessageId(),
            security_interstitials::common_string_util::GetFormattedHostName(
                request_url()),
            l10n_util::GetStringUTF16(
                IDS_ENTERPRISE_INTERSTITIALS_LEARN_MORE_ACCCESSIBILITY_TEXT)));
  }

  switch (type()) {
    case Type::kBlock:
      load_time_data.Set("enterprise-block", true);
      load_time_data.Set("type", "ENTERPRISE_BLOCK");
      load_time_data.Set("tabTitle",
                         l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_TITLE));
      load_time_data.Set(
          "heading", l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_HEADING));
      load_time_data.Set(
          "primaryButtonText",
          l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_GO_BACK));
      break;
    case Type::kWarn:
      load_time_data.Set("enterprise-warn", true);
      load_time_data.Set("type", "ENTERPRISE_WARN");
      load_time_data.Set("tabTitle",
                         l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_TITLE));
      load_time_data.Set(
          "heading", l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_HEADING));
      load_time_data.Set(
          "proceedButtonText",
          l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_CONTINUE_TO_SITE));
      load_time_data.Set("primaryButtonText", l10n_util::GetStringUTF16(
                                                  IDS_ENTERPRISE_WARN_GO_BACK));
      break;
  }
}

int EnterpriseInterstitialBase::GetPrimaryParagraphMessageId() const {
  switch (type()) {
    case Type::kBlock:
      return IDS_ENTERPRISE_BLOCK_PRIMARY_PARAGRAPH;
    case Type::kWarn:
      return IDS_ENTERPRISE_WARN_PRIMARY_PARAGRAPH;
  }
}

int EnterpriseInterstitialBase::GetCustomMessagePrimaryParagraphMessageId()
    const {
  switch (type()) {
    case Type::kBlock:
      return IDS_ENTERPRISE_BLOCK_PRIMARY_PARAGRAPH_CUSTOM_MESSAGE;
    case Type::kWarn:
      return IDS_ENTERPRISE_WARN_PRIMARY_PARAGRAPH_CUSTOM_MESSAGE;
  }
}

}  // namespace enterprise_connectors
