// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_

#include "base/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

// ImportLockDialogView asks the user to shut down Firefox before starting the
// profile import.
class ImportLockDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(ImportLockDialogView);
  ImportLockDialogView(const ImportLockDialogView&) = delete;
  ImportLockDialogView& operator=(const ImportLockDialogView&) = delete;

  static void Show(gfx::NativeWindow parent,
                   base::OnceCallback<void(bool)> callback);

 private:
  explicit ImportLockDialogView(base::OnceCallback<void(bool)> callback);
  ~ImportLockDialogView() override;

 private:
  // Called with the result of the dialog.
  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
