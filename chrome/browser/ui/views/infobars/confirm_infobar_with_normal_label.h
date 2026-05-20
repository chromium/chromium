// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_NORMAL_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_NORMAL_LABEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
}

class ConfirmInfoBarDelegate;

class ConfirmInfoBarWithNormalLabel : public ConfirmInfoBar {
  METADATA_HEADER(ConfirmInfoBarWithNormalLabel, ConfirmInfoBar)

 public:
  explicit ConfirmInfoBarWithNormalLabel(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

  ConfirmInfoBarWithNormalLabel(const ConfirmInfoBarWithNormalLabel&) = delete;
  ConfirmInfoBarWithNormalLabel& operator=(
      const ConfirmInfoBarWithNormalLabel&) = delete;

  ~ConfirmInfoBarWithNormalLabel() override;

  views::Label* label_for_testing() override;

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_NORMAL_LABEL_H_
