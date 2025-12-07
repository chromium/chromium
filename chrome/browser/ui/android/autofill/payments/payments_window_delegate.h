// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_DELEGATE_H_

class GURL;

namespace autofill::payments {

// Handles events from the ephemeral tab UI, such as web contents destruction
// and navigation completion.
class PaymentsWindowDelegate {
 public:
  virtual ~PaymentsWindowDelegate() = default;

  // Called when observation has started for the WebContents.
  virtual void OnWebContentsObservationStarted(
      content::WebContents& web_contents) = 0;

  // Triggered when the web contents of a tab shown as part of a window manager
  // flow was destroyed.
  virtual void WebContentsDestroyed() = 0;

  // Triggered when a tab navigation has finished, and `flow_state_->flow_type`
  // is `kBnpl`.
  virtual void OnDidFinishNavigationForBnpl(const GURL& clicked_url) = 0;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_DELEGATE_H_
