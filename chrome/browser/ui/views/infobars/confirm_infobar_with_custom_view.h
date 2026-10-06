// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_CUSTOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_CUSTOM_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}

class ConfirmInfoBarDelegate;

class ConfirmInfoBarWithCustomView : public ConfirmInfoBar {
  METADATA_HEADER(ConfirmInfoBarWithCustomView, ConfirmInfoBar)

 public:
  ConfirmInfoBarWithCustomView(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate,
      std::unique_ptr<views::View> custom_message_view);

  ConfirmInfoBarWithCustomView(const ConfirmInfoBarWithCustomView&) = delete;
  ConfirmInfoBarWithCustomView& operator=(const ConfirmInfoBarWithCustomView&) =
      delete;

  ~ConfirmInfoBarWithCustomView() override;

  views::View* custom_view_for_testing() { return custom_view_; }

 private:
  raw_ptr<views::View> custom_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_WITH_CUSTOM_VIEW_H_
