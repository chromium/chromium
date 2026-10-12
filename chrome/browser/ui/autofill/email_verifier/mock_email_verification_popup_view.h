// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_MOCK_EMAIL_VERIFICATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_MOCK_EMAIL_VERIFICATION_POPUP_VIEW_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockEmailVerificationPopupView : public EmailVerificationPopupView {
 public:
  MockEmailVerificationPopupView();
  MockEmailVerificationPopupView(const MockEmailVerificationPopupView&) =
      delete;
  MockEmailVerificationPopupView& operator=(
      const MockEmailVerificationPopupView&) = delete;
  ~MockEmailVerificationPopupView() override;

  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, ShowLoadingState, (), (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const, override));

  base::WeakPtr<MockEmailVerificationPopupView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void set_decision_callback(base::OnceCallback<void(bool)> callback) {
    decision_callback_ = std::move(callback);
  }
  void RunDecisionCallback(bool confirmed) {
    std::move(decision_callback_).Run(confirmed);
  }

 private:
  base::OnceCallback<void(bool)> decision_callback_;
  base::WeakPtrFactory<MockEmailVerificationPopupView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_MOCK_EMAIL_VERIFICATION_POPUP_VIEW_H_
