// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class WebContents;
}

namespace qrcode_generator {

class QRCodeGeneratorBubbleView;

// Controller component of the QR Code Generator dialog bubble.
// Responsible for showing and hiding an owned bubble.
class QRCodeGeneratorBubbleController
    : public content::WebContentsUserData<QRCodeGeneratorBubbleController> {
 public:
  ~QRCodeGeneratorBubbleController() override;

  static QRCodeGeneratorBubbleController* Get(
      content::WebContents* web_contents);

  // Displays the QR Code Generator bubble.
  void ShowBubble(const GURL& url);

  // Hides the QR Code Generator bubble.
  void HideBubble();

  // Returns nullptr if no bubble is currently shown.
  QRCodeGeneratorBubbleView* qrcode_generator_bubble_view() const;

  // Handler for when the bubble is dismissed.
  void OnBubbleClosed();

 protected:
  explicit QRCodeGeneratorBubbleController(content::WebContents* web_contents);

 private:
  QRCodeGeneratorBubbleController();

  friend class content::WebContentsUserData<QRCodeGeneratorBubbleController>;

  // The web_contents associated with this controller.
  content::WebContents* web_contents_;

  // Will be nullptr if no bubble is currently shown.
  QRCodeGeneratorBubbleView* qrcode_generator_bubble_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorBubbleController);
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
