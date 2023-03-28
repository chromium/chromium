// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace qrcode_generator {

// The location bar icon to show the QR Code generator bubble, where the user
// can generate a QR code for the current page or a selected image.
class QRCodeGeneratorIconView : public PageActionIconView {
 public:
  METADATA_HEADER(QRCodeGeneratorIconView);
  QRCodeGeneratorIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  QRCodeGeneratorIconView(const QRCodeGeneratorIconView&) = delete;
  QRCodeGeneratorIconView& operator=(const QRCodeGeneratorIconView&) = delete;
  ~QRCodeGeneratorIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  bool ShouldShowLabel() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  // Flag set when OnExecuting() is called and cleared after bubble is created.
  // Avoids losing ink drop on, or flickering, the PageActionIconView.
  bool bubble_requested_;
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
