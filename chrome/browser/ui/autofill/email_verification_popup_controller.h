// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFICATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFICATION_POPUP_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/schemeful_site.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {
class Widget;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class EmailVerificationPopupView;

// Controller for the email verification popup. It manages the lifecycle of
// the popup widget and handles the user's decision.
class EmailVerificationPopupController
    : public autofill::AutofillPopupViewDelegate,
      public content::WebContentsObserver {
 public:
  explicit EmailVerificationPopupController(content::WebContents* web_contents);
  EmailVerificationPopupController(const EmailVerificationPopupController&) =
      delete;
  EmailVerificationPopupController& operator=(
      const EmailVerificationPopupController&) = delete;
  ~EmailVerificationPopupController() override;

  // Shows the email verification popup anchored to the element bounds.
  // `callback` is invoked with the user's decision
  // (AutofillClient::EmailVerificationPermissionUiResult).
  void Show(const gfx::RectF& element_bounds,
            const net::SchemefulSite& issuer,
            const std::u16string& email,
            base::OnceCallback<void(
                AutofillClient::EmailVerificationPermissionUiResult)> callback);

  // autofill::AutofillPopupViewDelegate:
  void Hide(autofill::SuggestionHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  autofill::PopupAnchorType anchor_type() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // content::WebContentsObserver:
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  base::WeakPtr<EmailVerificationPopupController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  using ViewFactoryForTesting =
      base::RepeatingCallback<base::WeakPtr<EmailVerificationPopupView>(
          base::WeakPtr<EmailVerificationPopupController> delegate,
          views::Widget* parent_widget,
          const net::SchemefulSite& issuer,
          const std::u16string& email,
          base::OnceCallback<void(bool)> callback)>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(EvpPermissionUiStatus)
  enum class EvpPermissionUiStatus {
    kAllowed = 0,
    kDeclined = 1,
    kUserAborted = 2,            // e.g. ESC key or clicking outside
    kNavigation = 3,             // page navigated
    kTabGone = 4,                // tab closed or hidden
    kWidgetChanged = 5,          // e.g. window resized
    kOverlappingPrompt = 6,      // overlapped by another prompt/pip
    kOther = 7,                  // any other reason
    kViewDestroyedDirectly = 8,  // view destroyed without explicit Hide()
    kMaxValue = kViewDestroyedDirectly,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:EvpPermissionUiStatus)

  void set_view_factory_for_testing(ViewFactoryForTesting factory) {
    view_factory_for_testing_ = std::move(factory);
  }

 private:
  void OnConfirm();
  void OnCancel();
  void HideImpl(AutofillClient::EmailVerificationPermissionUiResult result,
                EvpPermissionUiStatus status);
  bool OverlapsWithPictureInPictureWindow() const;

  // The bounds of the element that triggered the popup.
  gfx::RectF element_bounds_;

  // The callback to invoke with the user's decision.
  base::OnceCallback<void(AutofillClient::EmailVerificationPermissionUiResult)>
      callback_;

  // The view representing the popup.
  base::WeakPtr<EmailVerificationPopupView> view_;

  // Helper to handle hiding the popup on various events.
  std::optional<autofill::AutofillPopupHideHelper> popup_hide_helper_;

  // Factory function used to create the view in tests.
  ViewFactoryForTesting view_factory_for_testing_;

  base::WeakPtrFactory<EmailVerificationPopupController> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFICATION_POPUP_CONTROLLER_H_
