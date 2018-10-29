// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_select_file_dialog_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/win/win_util.h"
#include "chrome/services/util_win/public/mojom/constants.mojom.h"
#include "chrome/services/util_win/public/mojom/shell_util_win.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/shell_dialogs/execute_select_file_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

// This feature controls whether or not file dialogs are executed in a utility
// process on Windows.
base::Feature kWinOOPSelectFileDialog{"WinOOPSelectFileDialog",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace

std::unique_ptr<service_manager::Connector> GetConnectorOnUIThread() {
  return content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->Clone();
}
// static

// Helper class to execute a select file operation on a utility process. It
// hides the complexity of managing the lifetime of the connection to the
// ChromeWinUtil service.
class ShellUtilWinHelper {
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
  ShellUtilWinHelper(
      ui::SelectFileDialog::Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const std::vector<ui::FileFilterSpec>& filter,
      int file_type_index,
      const base::string16& default_extension,
      HWND owner,
      ui::OnSelectFileExecutedCallback on_select_file_executed_callback);

  // Invoked back on the sequence this instance was created on when the
  // connector is received from the UI thread.
  void OnConnectorReceived(
      ui::SelectFileDialog::Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const std::vector<ui::FileFilterSpec>& filter,
      int file_type_index,
      const base::string16& default_extension,
      HWND owner,
      std::unique_ptr<service_manager::Connector> connector);

  // Connection error handler for the interface pipe.
  void OnConnectionError();

  // Forwards the result of the file operation to the
  // |on_select_file_executed_callback_|.
  void OnSelectFileExecuted(const std::vector<base::FilePath>& paths,
                            int index);

  // The pointer to the ShellUtilWin interface. This must be kept alive while
  // waiting for the response.
  chrome::mojom::ShellUtilWinPtr shell_util_win_ptr_;

  // The callback that is invoked when the file operation is finished.
  ui::OnSelectFileExecutedCallback on_select_file_executed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ShellUtilWinHelper);
};

// static
void ShellUtilWinHelper::ExecuteSelectFile(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner,
    ui::OnSelectFileExecutedCallback on_select_file_executed_callback) {
  // Self-deleting when the select file operation completes.
  new ShellUtilWinHelper(type, title, default_path, filter, file_type_index,
                         default_extension, owner,
                         std::move(on_select_file_executed_callback));
}

ShellUtilWinHelper::ShellUtilWinHelper(
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
  // A valid connector is required to create the interface pointer.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&GetConnectorOnUIThread),
      base::BindOnce(&ShellUtilWinHelper::OnConnectorReceived,
                     base::Unretained(this), type, title, default_path, filter,
                     file_type_index, default_extension, owner));
}

void ShellUtilWinHelper::OnConnectorReceived(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner,
    std::unique_ptr<service_manager::Connector> connector) {
  connector->BindInterface(chrome::mojom::kUtilWinServiceName,
                           &shell_util_win_ptr_);

  // |shell_util_win_ptr_| owns the callbacks and is guaranteed to be destroyed
  // before |this|, therefore making base::Unretained() safe to use.
  shell_util_win_ptr_.set_connection_error_handler(base::BindOnce(
      &ShellUtilWinHelper::OnConnectionError, base::Unretained(this)));

  shell_util_win_ptr_->CallExecuteSelectFile(
      type, base::win::HandleToUint32(owner), title, default_path, filter,
      file_type_index, default_extension,
      base::BindOnce(&ShellUtilWinHelper::OnSelectFileExecuted,
                     base::Unretained(this)));
}

void ShellUtilWinHelper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(on_select_file_executed_callback_).Run({}, 0);

  base::UmaHistogramBoolean("Windows.OOPSelectFileDialog.ProcessError", true);

  delete this;
}

void ShellUtilWinHelper::OnSelectFileExecuted(
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
  if (!base::FeatureList::IsEnabled(kWinOOPSelectFileDialog)) {
    ui::ExecuteSelectFile(type, title, default_path, filter, file_type_index,
                          default_extension, owner,
                          std::move(on_select_file_executed_callback));
    return;
  }

  ShellUtilWinHelper::ExecuteSelectFile(
      type, title, default_path, filter, file_type_index, default_extension,
      owner, std::move(on_select_file_executed_callback));
}

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory() = default;

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() = default;

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return ui::CreateWinSelectFileDialog(
      listener, std::move(policy), base::BindRepeating(&ExecuteSelectFileImpl));
}
