// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_

#include "base/functional/callback.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

// ImportLockDialogView asks the user to shut down the source browser (default
// Firefox) before starting the profile import.
class ImportLockDialogView : public views::DialogDelegateView {
  METADATA_HEADER(ImportLockDialogView, views::DialogDelegateView)

 public:
  ImportLockDialogView(const ImportLockDialogView&) = delete;
  ImportLockDialogView& operator=(const ImportLockDialogView&) = delete;

  static void Show(gfx::NativeWindow parent,
                   base::OnceCallback<void(bool)> callback,
                   int importer_lock_title_id = IDS_IMPORTER_LOCK_TITLE,
                   int importer_lock_text_id = IDS_IMPORTER_LOCK_TEXT);

 private:
  ImportLockDialogView(base::OnceCallback<void(bool)> callback,
                       int importer_lock_title_id,
                       int importer_lock_text_id);
  ~ImportLockDialogView() override;

 private:
  // Called with the result of the dialog.
  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
