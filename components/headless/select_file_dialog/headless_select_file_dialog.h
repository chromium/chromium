// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_SELECT_FILE_DIALOG_HEADLESS_SELECT_FILE_DIALOG_H_
#define COMPONENTS_HEADLESS_SELECT_FILE_DIALOG_HEADLESS_SELECT_FILE_DIALOG_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace headless {

using SelectFileDialogCallback =
    base::OnceCallback<void(ui::SelectFileDialog::Type type)>;

class COMPONENT_EXPORT(HEADLESS) HeadlessSelectFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  HeadlessSelectFileDialogFactory(const HeadlessSelectFileDialogFactory&) =
      delete;
  HeadlessSelectFileDialogFactory& operator=(
      const HeadlessSelectFileDialogFactory&) = delete;

  ~HeadlessSelectFileDialogFactory() override;

  // Creates the factory and sets it into ui::SelectFileDialog.
  static void SetUp();

  // Registers a one time callback that would be called when the next Select
  // File Dialog comes up. This can only be called after SetUp().
  static void SetSelectFileDialogOnceCallbackForTests(
      SelectFileDialogCallback callback);

 private:
  friend class HeadlessSelectFileDialog;

  // ui::SelectFileDialogFactory
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

  HeadlessSelectFileDialogFactory();

  static HeadlessSelectFileDialogFactory* instance_;
  SelectFileDialogCallback callback_;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_SELECT_FILE_DIALOG_HEADLESS_SELECT_FILE_DIALOG_H_
