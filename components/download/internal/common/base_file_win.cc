// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <objbase.h>

#include <shobjidl.h>
#include <windows.h>

#include <shellapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_stats.h"

namespace download {
namespace {

// By and large the errors seen here are listed in sherrors.h, included from
// shobjidl.h.
DownloadInterruptReason HRESULTToDownloadInterruptReason(HRESULT hr) {
  // S_OK, other success values are aggregated here.
  if (SUCCEEDED(hr) && HRESULT_FACILITY(hr) != FACILITY_SHELL)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  // All of the remaining HRESULTs to be considered are either from the copy
  // engine, or are unknown; we've got handling for all the copy engine errors,
  // and otherwise we'll just return the generic error reason.
  switch (hr) {
    case COPYENGINE_S_YES:
    case COPYENGINE_S_NOT_HANDLED:
    case COPYENGINE_S_USER_RETRY:
    case COPYENGINE_S_MERGE:
    case COPYENGINE_S_DONT_PROCESS_CHILDREN:
    case COPYENGINE_S_ALREADY_DONE:
    case COPYENGINE_S_PENDING:
    case COPYENGINE_S_KEEP_BOTH:
    case COPYENGINE_S_COLLISIONRESOLVED:
    case COPYENGINE_S_PROGRESS_PAUSE:
      return DOWNLOAD_INTERRUPT_REASON_NONE;

    case COPYENGINE_S_CLOSE_PROGRAM:
      // Like sharing violations, another process is using the file we want to
      // touch, so wait for it to close.
    case COPYENGINE_E_SHARING_VIOLATION_SRC:
    case COPYENGINE_E_SHARING_VIOLATION_DEST:
      // Sharing violations are encountered when some other process has a file
      // open; often it's antivirus scanning, and this error can be treated as
      // transient, as we assume eventually the other process will close its
      // handle.
      reason = DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
      break;

    case COPYENGINE_E_PATH_TOO_DEEP_DEST:
    case COPYENGINE_E_PATH_TOO_DEEP_SRC:
    case COPYENGINE_E_NEWFILE_NAME_TOO_LONG:
    case COPYENGINE_E_NEWFOLDER_NAME_TOO_LONG:
      // Any of these errors can be encountered if MAXPATH is hit while writing
      // out a filename. This can happen really just about anywhere.
      reason = DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;
      break;

    case COPYENGINE_S_USER_IGNORED:
      // On Windows 7, inability to access a file may return "user ignored"
      // instead of correctly reporting the failure.
    case COPYENGINE_E_ACCESS_DENIED_DEST:
    case COPYENGINE_E_ACCESS_DENIED_SRC:
      // There's a security problem, or the file is otherwise inaccessible.
    case COPYENGINE_E_DEST_IS_RO_CD:
    case COPYENGINE_E_DEST_IS_RW_CD:
    case COPYENGINE_E_DEST_IS_R_CD:
    case COPYENGINE_E_DEST_IS_RO_DVD:
    case COPYENGINE_E_DEST_IS_RW_DVD:
    case COPYENGINE_E_DEST_IS_R_DVD:
    case COPYENGINE_E_SRC_IS_RO_CD:
    case COPYENGINE_E_SRC_IS_RW_CD:
    case COPYENGINE_E_SRC_IS_R_CD:
    case COPYENGINE_E_SRC_IS_RO_DVD:
    case COPYENGINE_E_SRC_IS_RW_DVD:
    case COPYENGINE_E_SRC_IS_R_DVD:
      // When the source is actually a disk, and a Move is attempted, it can't
      // delete the source. This is unlikely to be encountered in our scenario.
      reason = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    case COPYENGINE_E_FILE_TOO_LARGE:
    case COPYENGINE_E_DISK_FULL:
    case COPYENGINE_E_REMOVABLE_FULL:
    case COPYENGINE_E_DISK_FULL_CLEAN:
      // No room for the file in the destination location.
      reason = DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE;
      break;

    case COPYENGINE_E_ALREADY_EXISTS_NORMAL:
    case COPYENGINE_E_ALREADY_EXISTS_READONLY:
    case COPYENGINE_E_ALREADY_EXISTS_SYSTEM:
    case COPYENGINE_E_ALREADY_EXISTS_FOLDER:
      // The destination already exists and can't be replaced.
    case COPYENGINE_E_INVALID_FILES_SRC:
    case COPYENGINE_E_INVALID_FILES_DEST:
      // Either the source or destination file was invalid.
    case COPYENGINE_E_STREAM_LOSS:
    case COPYENGINE_E_EA_LOSS:
    case COPYENGINE_E_PROPERTY_LOSS:
    case COPYENGINE_E_PROPERTIES_LOSS:
    case COPYENGINE_E_ENCRYPTION_LOSS:
      // The destination can't support some functionality that the file needs.
      // The interesting one here is E_STREAM_LOSS, especially with MOTW.
    case COPYENGINE_E_FLD_IS_FILE_DEST:
    case COPYENGINE_E_FILE_IS_FLD_DEST:
      // There is an existing file with the same name as a new folder, and
      // vice versa.
    case COPYENGINE_E_ROOT_DIR_DEST:
    case COPYENGINE_E_ROOT_DIR_SRC:
    case COPYENGINE_E_DIFF_DIR:
    case COPYENGINE_E_SAME_FILE:
    case COPYENGINE_E_MANY_SRC_1_DEST:
    case COPYENGINE_E_DEST_SUBTREE:
    case COPYENGINE_E_DEST_SAME_TREE:
    case COPYENGINE_E_USER_CANCELLED:
    case COPYENGINE_E_CANCELLED:
    case COPYENGINE_E_REQUIRES_ELEVATION:
      reason = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;
  }

  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    return reason;
  }

  // Copy operations may still return Win32 error codes, so handle those here.
  if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
    return ConvertFileErrorToInterruptReason(
        base::File::OSErrorToFileError(HRESULT_CODE(hr)));
  }

  return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

class FileOperationProgressSink
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IFileOperationProgressSink> {
 public:
  FileOperationProgressSink() = default;

  FileOperationProgressSink(const FileOperationProgressSink&) = delete;
  FileOperationProgressSink& operator=(const FileOperationProgressSink&) =
      delete;

  HRESULT GetOperationResult() { return result_; }

  // IFileOperationProgressSink:
  IFACEMETHODIMP FinishOperations(HRESULT hr) override {
    // If a failure has already been captured, don't bother overriding it. That
    // way, the original failure can be propagated; in the event that the new
    // HRESULT is also a success, overwriting will not harm anything and
    // captures the final state of the whole operation.
    if (SUCCEEDED(result_))
      result_ = hr;
    return S_OK;
  }

  IFACEMETHODIMP PauseTimer() override { return S_OK; }
  IFACEMETHODIMP PostCopyItem(DWORD,
                              IShellItem*,
                              IShellItem*,
                              PCWSTR,
                              HRESULT,
                              IShellItem*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PostDeleteItem(DWORD,
                                IShellItem*,
                                HRESULT,
                                IShellItem*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PostMoveItem(DWORD,
                              IShellItem*,
                              IShellItem*,
                              PCWSTR,
                              HRESULT hr,
                              IShellItem*) override {
    // Like in FinishOperations, overwriting with a different success value
    // does not have a negative impact, but replacing an existing failure will
    // cause issues.
    if (SUCCEEDED(result_))
      result_ = hr;
    return S_OK;
  }
  IFACEMETHODIMP PostNewItem(DWORD,
                             IShellItem*,
                             PCWSTR,
                             PCWSTR,
                             DWORD,
                             HRESULT,
                             IShellItem*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  PostRenameItem(DWORD, IShellItem*, PCWSTR, HRESULT, IShellItem*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PreCopyItem(DWORD, IShellItem*, IShellItem*, PCWSTR) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PreDeleteItem(DWORD, IShellItem*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PreMoveItem(DWORD, IShellItem*, IShellItem*, PCWSTR) override {
    return S_OK;
  }
  IFACEMETHODIMP PreNewItem(DWORD, IShellItem*, PCWSTR) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP PreRenameItem(DWORD, IShellItem*, PCWSTR) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP ResetTimer() override { return S_OK; }
  IFACEMETHODIMP ResumeTimer() override { return S_OK; }
  IFACEMETHODIMP StartOperations() override { return S_OK; }
  IFACEMETHODIMP UpdateProgress(UINT, UINT) override { return S_OK; }

 protected:
  ~FileOperationProgressSink() override = default;

 private:
  HRESULT result_ = S_OK;
};

}  // namespace

// Renames a file using IFileOperation::MoveItem() to ensure that the target
// file gets the correct default security descriptor in the new path.
// Returns a network error, or net::OK for success.
DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IShellItem> original_path;
  HRESULT hr = SHCreateItemFromParsingName(full_path_.value().c_str(), nullptr,
                                           IID_PPV_ARGS(&original_path));

  // |new_path| can be broken down to provide the new folder, as well as the
  // new filename. We'll start with the folder, which the caller should ensure
  // exists.
  Microsoft::WRL::ComPtr<IShellItem> destination_folder;
  if (SUCCEEDED(hr)) {
    hr =
        SHCreateItemFromParsingName(new_path.DirName().value().c_str(), nullptr,
                                    IID_PPV_ARGS(&destination_folder));
  }

  Microsoft::WRL::ComPtr<IFileOperation> file_operation;
  if (SUCCEEDED(hr)) {
    hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&file_operation));
  }

  if (SUCCEEDED(hr)) {
    // Don't show any UI, don't migrate security attributes (use the
    // destination's attributes), and stop on first error-retaining the original
    // failure reason.
    hr = file_operation->SetOperationFlags(
        FOF_NO_UI | FOF_NOCOPYSECURITYATTRIBS | FOFX_EARLYFAILURE);
  }

  Microsoft::WRL::ComPtr<FileOperationProgressSink> sink =
      Microsoft::WRL::Make<FileOperationProgressSink>();
  if (SUCCEEDED(hr)) {
    hr = file_operation->MoveItem(original_path.Get(), destination_folder.Get(),
                                  new_path.BaseName().value().c_str(),
                                  sink.Get());
  }

  if (SUCCEEDED(hr))
    hr = file_operation->PerformOperations();

  if (SUCCEEDED(hr))
    hr = sink->GetOperationResult();

  // Convert HRESULT to DownloadInterruptReason.
  DownloadInterruptReason interrupt_reason =
      HRESULTToDownloadInterruptReason(hr);

  if (interrupt_reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
    // The operation could still have been aborted; we can't get a better reason
    // at this point, but we've got more information to go by.
    BOOL any_operations_aborted = TRUE;
    file_operation->GetAnyOperationsAborted(&any_operations_aborted);
    if (any_operations_aborted)
      interrupt_reason = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  } else {
    return LogInterruptReason("IFileOperation::MoveItem", hr, interrupt_reason);
  }

  return interrupt_reason;
}

}  // namespace download
