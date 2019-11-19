// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace qrcode_generator {

// The location bar icon to show the QR Code generator bubble, where the user
// can generate a QR code for the current page or a selected image.
class QRCodeGeneratorIconView : public PageActionIconView {
 public:
  QRCodeGeneratorIconView(CommandUpdater* command_updater,
                          PageActionIconView::Delegate* delegate);
  ~QRCodeGeneratorIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  bool Update() override;
  SkColor GetTextColor() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorIconView);
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
