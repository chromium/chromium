// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_WINDOW_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace qrcode_generator {

class QRCodeGeneratorBubbleView;

// Manages the QR Code Generator bubble for a browser window.
class QRCodeWindowController {
 public:
  explicit QRCodeWindowController(BrowserWindowInterface* browser);
  QRCodeWindowController(const QRCodeWindowController&) = delete;
  QRCodeWindowController& operator=(const QRCodeWindowController&) = delete;
  ~QRCodeWindowController();

  DECLARE_USER_DATA(QRCodeWindowController);

  static QRCodeWindowController* From(BrowserWindowInterface* browser);

  // Shows the QR Code Generator bubble.
  QRCodeGeneratorBubbleView* ShowBubble(content::WebContents* contents,
                                        const GURL& url,
                                        bool show_back_button);

 private:
  friend class ui::ScopedUnownedUserData<QRCodeWindowController>;

  const raw_ref<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<QRCodeWindowController> scoped_unowned_user_data_;
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_WINDOW_CONTROLLER_H_
