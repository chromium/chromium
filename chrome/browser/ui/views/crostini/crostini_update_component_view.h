// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_COMPONENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_COMPONENT_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

class Profile;

// Provides a warning to the user that an upgrade is required and and internet
// connection is needed.
class CrostiniUpdateComponentView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CrostiniUpdateComponentView);

  static void Show(Profile* profile);

  static CrostiniUpdateComponentView* GetActiveViewForTesting();

 private:
  CrostiniUpdateComponentView();
  ~CrostiniUpdateComponentView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_COMPONENT_VIEW_H_
