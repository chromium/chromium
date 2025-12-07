// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
}  // namespace views

// UninstallView implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile.
class UninstallView : public views::DialogDelegateView {
  METADATA_HEADER(UninstallView, views::DialogDelegateView)

 public:
  explicit UninstallView(int* user_selection,
                         const base::RepeatingClosure& quit_closure);
  UninstallView(const UninstallView&) = delete;
  UninstallView& operator=(const UninstallView&) = delete;
  ~UninstallView() override;

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  void OnDialogAccepted();
  void OnDialogCancelled();

  raw_ptr<views::Checkbox> delete_profile_ = nullptr;
  const raw_ref<int> user_selection_;
  base::RepeatingClosure quit_closure_;
};

BEGIN_VIEW_BUILDER(, UninstallView, views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, UninstallView)

#endif  // CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
