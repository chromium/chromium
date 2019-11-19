// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_select_file_dialog_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/win/win_util.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/shell_dialogs/execute_select_file_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

// Helper class to execute a select file operation on a utility process. It
// hides the complexity of managing the lifetime of the connection to the
// UtilWin service.
class UtilWinHelper {
 public:
  // Executes the select file operation and returns the result via
  // |on_select_file_executed_callback|.
  static void ExecuteSelectFile(
      ui::SelectFileDialog::Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const std::vector<ui::FileFilterSpec>& filter,
      int file_type_index,
      const base::string16& default_extension,
      HWND owner,
      ui::OnSelectFileExecutedCallback on_select_file_executed_callback);

 private:
  UtilWinHelper(
      ui::SelectFileDialog::Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const std::vector<ui::FileFilterSpec>& filter,
      int file_type_index,
      const base::string16& default_extension,
      HWND owner,
      ui::OnSelectFileExecutedCallback on_select_file_executed_callback);

  // Connection error handler for the interface pipe.
  void OnConnectionError();

  // Forwards the result of the file operation to the
  // |on_select_file_executed_callback_|.
  void OnSelectFileExecuted(const std::vector<base::FilePath>& paths,
                            int index);

  // The pointer to the UtilWin interface. This must be kept alive while waiting
  // for the response.
  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_;

  // The callback that is invoked when the file operation is finished.
  ui::OnSelectFileExecutedCallback on_select_file_executed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(UtilWinHelper);
};

// static
void UtilWinHelper::ExecuteSelectFile(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner,
    ui::OnSelectFileExecutedCallback on_select_file_executed_callback) {
  // Self-deleting when the select file operation completes.
  new UtilWinHelper(type, title, default_path, filter, file_type_index,
                    default_extension, owner,
                    std::move(on_select_file_executed_callback));
}

UtilWinHelper::UtilWinHelper(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner,
    ui::OnSelectFileExecutedCallback on_select_file_executed_callback)
    : on_select_file_executed_callback_(
          std::move(on_select_file_executed_callback)) {
  remote_util_win_ = LaunchUtilWinServiceInstance();

  // |remote_util_win_| owns the callbacks and is guaranteed to be destroyed
  // before |this|, therefore making base::Unretained() safe to use.
  remote_util_win_.set_disconnect_handler(base::BindOnce(
      &UtilWinHelper::OnConnectionError, base::Unretained(this)));

  remote_util_win_->CallExecuteSelectFile(
      type, base::win::HandleToUint32(owner), title, default_path, filter,
      file_type_index, default_extension,
      base::BindOnce(&UtilWinHelper::OnSelectFileExecuted,
                     base::Unretained(this)));
}

void UtilWinHelper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(on_select_file_executed_callback_).Run({}, 0);

  base::UmaHistogramBoolean("Windows.OOPSelectFileDialog.ProcessError", true);

  delete this;
}

void UtilWinHelper::OnSelectFileExecuted(
    const std::vector<base::FilePath>& paths,
    int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean("Windows.OOPSelectFileDialog.ProcessError", false);

  std::move(on_select_file_executed_callback_).Run(paths, index);
  delete this;
}

void ExecuteSelectFileImpl(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner,
    ui::OnSelectFileExecutedCallback on_select_file_executed_callback) {
  UtilWinHelper::ExecuteSelectFile(type, title, default_path, filter,
                                   file_type_index, default_extension, owner,
                                   std::move(on_select_file_executed_callback));
}

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory() = default;

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() = default;

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return ui::CreateWinSelectFileDialog(
      listener, std::move(policy), base::BindRepeating(&ExecuteSelectFileImpl));
}
