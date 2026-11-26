// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TTC_TTC_FRE_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_TTC_TTC_FRE_DIALOG_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace ttc {

// The window-modal dialog asking for the user's consent before starting the
// TTC session. Consent is required whenever the user has not accepted the
// current version of the TTC FRE.
class TtcFreDialog {
 public:
  using ResultCallback = base::OnceCallback<void(bool accepted)>;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDialogElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLearnMoreLinkElementId);

  // `on_closed` runs once: with true on Allow, or with false on Cancel, Esc or
  // when `parent` closes. It may delete the dialog. `on_learn_more` runs each
  // time the user activates the "Learn more" link; the dialog stays open.
  TtcFreDialog(gfx::NativeWindow parent,
               ResultCallback on_closed,
               base::RepeatingClosure on_learn_more);
  TtcFreDialog(const TtcFreDialog&) = delete;
  TtcFreDialog& operator=(const TtcFreDialog&) = delete;

  // Closes the dialog, if still open, without running `on_closed`.
  ~TtcFreDialog();

  void Show();
  void Activate();

  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  void OnClosed(views::Widget::ClosedReason reason);

  ResultCallback on_closed_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<TtcFreDialog> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_VIEWS_TTC_TTC_FRE_DIALOG_H_
