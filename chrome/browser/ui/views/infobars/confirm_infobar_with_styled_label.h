// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_STYLED_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_STYLED_LABEL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class StyledLabel;
}

// A specialized subclass of ConfirmInfoBar that supports inline links in the
// message text via StyledLabel.
class ConfirmInfoBarWithStyledLabel : public ConfirmInfoBar {
  METADATA_HEADER(ConfirmInfoBarWithStyledLabel, ConfirmInfoBar)

 public:
  explicit ConfirmInfoBarWithStyledLabel(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  ConfirmInfoBarWithStyledLabel(const ConfirmInfoBarWithStyledLabel&) = delete;
  ConfirmInfoBarWithStyledLabel& operator=(
      const ConfirmInfoBarWithStyledLabel&) = delete;

  ~ConfirmInfoBarWithStyledLabel() override;

  views::StyledLabel* styled_label_for_testing() { return styled_label_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ConfirmInfoBarWithInlineLinksTest,
                           TemplateMessageUsesStyledLabel);

  raw_ptr<views::StyledLabel> styled_label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_STYLED_LABEL_H_
