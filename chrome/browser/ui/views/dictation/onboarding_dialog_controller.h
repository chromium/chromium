// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_ONBOARDING_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_ONBOARDING_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget.h"

namespace tabs {
class TabInterface;
}

namespace ui {
class DialogModel;
}

namespace views {
class Widget;
}

namespace dictation {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingDialogElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingOkButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingCancelButtonElementId);

// Manages the Dictation onboarding dialog UI.
class OnboardingDialogController {
 public:
  explicit OnboardingDialogController(tabs::TabInterface& tab);
  OnboardingDialogController(const OnboardingDialogController&) = delete;
  OnboardingDialogController& operator=(const OnboardingDialogController&) =
      delete;
  ~OnboardingDialogController();

  // Shows the onboarding dialog.
  // `complete_callback` is called when the user accepts the onboarding.
  // `close_callback` is called when the dialog is closed (cancelled or
  // dismissed).
  void Show(base::OnceClosure complete_callback,
            base::OnceClosure close_callback);

  bool IsShowing() const;

 private:
  std::unique_ptr<ui::DialogModel> CreateDialogModel(
      base::OnceClosure complete_callback);
  void OnDialogAccepted(base::OnceClosure complete_callback);
  void OnLearnMoreLinkClicked();
  void Close(views::Widget::ClosedReason reason);

  const raw_ref<tabs::TabInterface> tab_;
  std::unique_ptr<views::Widget> widget_;
  base::OnceClosure close_callback_;

  base::WeakPtrFactory<OnboardingDialogController> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_ONBOARDING_DIALOG_CONTROLLER_H_
