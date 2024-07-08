// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class PrefChangeRegistrar;

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
  QRCodeGeneratorBubbleController(const QRCodeGeneratorBubbleController&) =
      delete;
  QRCodeGeneratorBubbleController& operator=(
      const QRCodeGeneratorBubbleController&) = delete;

  ~QRCodeGeneratorBubbleController() override;

  // Returns whether the generator is available for a given page.
  static bool IsGeneratorAvailable(const GURL& url);

  static QRCodeGeneratorBubbleController* Get(
      content::WebContents* web_contents);

  // Displays the QR Code Generator bubble.
  void ShowBubble(const GURL& url, bool show_back_button = false);

  // Hides the QR Code Generator bubble.
  void HideBubble();

  bool IsBubbleShown() { return bubble_shown_; }

  // Returns nullptr if no bubble is currently shown.
  QRCodeGeneratorBubbleView* qrcode_generator_bubble_view() const;

  base::OnceClosure GetOnBubbleClosedCallback();
  base::OnceClosure GetOnBackButtonPressedCallback();

 protected:
  explicit QRCodeGeneratorBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<QRCodeGeneratorBubbleController>;

  // Handler for when the bubble is dismissed.
  void OnBubbleClosed();
  // Handler for when the back button is pressed.
  void OnBackButtonPressed();

  // Hides the bubble if the policy controlling the QR code generator is
  // disabled.
  void OnPolicyPrefChanged();

  // When the bubble is visible, used for tracking changes to the policy
  // controlling the QR code generator.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Will be nullptr if no bubble is currently shown.
  raw_ptr<QRCodeGeneratorBubbleView> qrcode_generator_bubble_ = nullptr;

  // True if the bubble is currently shown.
  bool bubble_shown_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<QRCodeGeneratorBubbleController> weak_ptr_factory_{this};
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
