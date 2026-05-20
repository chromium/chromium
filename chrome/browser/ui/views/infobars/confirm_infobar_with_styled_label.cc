// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar_with_styled_label.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/styled_label.h"

ConfirmInfoBarWithStyledLabel::ConfirmInfoBarWithStyledLabel(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();

  // Retrieve the message template,
  // substitution strings, and link metadata from the delegate.
  const std::u16string message_template =
      delegate_ptr->GetMessageTextTemplate();
  const std::vector<MessageSubstitution> substitutions =
      delegate_ptr->GetMessageSubstitutions();

  std::vector<std::u16string> substitution_strings;
  for (const auto& sub : substitutions) {
    substitution_strings.push_back(sub.text);
  }

  // Perform placeholder substitution to build the final message string,
  // tracking offsets where each substitution lands in the resulting string.
  std::vector<size_t> offsets;
  const std::u16string text = base::ReplaceStringPlaceholders(
      message_template, substitution_strings, &offsets);

  CHECK_EQ(offsets.size(), substitutions.size());

  auto styled_label = CreateStyledLabel(text);

  // For each substitution, if it's marked as a link, add the appropriate
  // style range to the StyledLabel.
  for (size_t i = 0; i < offsets.size(); ++i) {
    const auto& sub = substitutions[i];
    if (sub.is_link) {
      views::StyledLabel::RangeStyleInfo style_info =
          views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
              [](ConfirmInfoBarWithStyledLabel* infobar, size_t index,
                 const ui::Event& event) {
                if (!infobar->owner()) {
                  return;
                }
                // Handle link click, passing the disposition derived from
                // the event flags.
                if (infobar->GetDelegate()->InlineSubstitutionLinkClicked(
                        index, ui::DispositionFromEventFlags(event.flags()))) {
                  infobar->RemoveSelf();
                }
              },
              base::Unretained(this), i));

      // Assign custom accessibility name for the link if one is provided.
      if (sub.accessible_name.has_value()) {
        style_info.accessible_name = sub.accessible_name.value();
      }

      // Apply the link style range to the corresponding substring.
      styled_label->AddStyleRange(
          gfx::Range(offsets[i], offsets[i] + sub.text.length()), style_info);
    }
  }

  styled_label_ = AssignMessageLabel(std::move(styled_label));
}

ConfirmInfoBarWithStyledLabel::~ConfirmInfoBarWithStyledLabel() = default;

BEGIN_METADATA(ConfirmInfoBarWithStyledLabel)
END_METADATA
