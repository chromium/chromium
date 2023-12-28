// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Combobox;
}  // namespace views

// UninstallView implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile. Also if currently Chrome is set
// as default browser, it asks users whether to set another browser as default.
class UninstallView : public views::DialogDelegateView,
                      public ui::ComboboxModel {
  METADATA_HEADER(UninstallView, views::DialogDelegateView)

 public:
  explicit UninstallView(int* user_selection,
                         const base::RepeatingClosure& quit_closure);
  UninstallView(const UninstallView&) = delete;
  UninstallView& operator=(const UninstallView&) = delete;
  ~UninstallView() override;

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;

 private:
  typedef std::map<std::wstring, std::wstring> BrowsersMap;

  // Initializes the controls on the dialog.
  void SetupControls();

  void OnDialogAccepted();
  void OnDialogCancelled();

  raw_ptr<views::Checkbox> delete_profile_ = nullptr;
  raw_ptr<views::Checkbox> change_default_browser_ = nullptr;
  raw_ptr<views::Combobox> browsers_combo_ = nullptr;
  std::unique_ptr<BrowsersMap> browsers_;
  const raw_ref<int> user_selection_;
  base::RepeatingClosure quit_closure_;
};

BEGIN_VIEW_BUILDER(, UninstallView, views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, UninstallView)

#endif  // CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
