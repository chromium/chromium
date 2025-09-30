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
#include "base/win/windows_version.h"
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

// Maps the result of a call to |SHFileOperation()| onto a
// |DownloadInterruptReason|.
//
// These return codes are *old* (as in, DOS era), and specific to
// |SHFileOperation()|.
// They do not appear in any windows header.
//
// See http://msdn.microsoft.com/en-us/library/bb762164(VS.85).aspx.
DownloadInterruptReason MapShFileOperationCodes(int code) {
  DownloadInterruptReason result = DOWNLOAD_INTERRUPT_REASON_NONE;

  // Check these pre-Win32 error codes first, then check for matches
  // in Winerror.h.
  // This switch statement should be kept in sync with the list of codes
  // above.
  switch (code) {
    // Not a pre-Win32 error code; here so that this particular case shows up in
    // our histograms. Unfortunately, it is used not just to signal actual
    // ACCESS_DENIED errors, but many other errors as well. So we treat it as a
    // transient error.
    case ERROR_ACCESS_DENIED:  // Access is denied.
      result = DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
      break;

    // This isn't documented but returned from SHFileOperation. Sharing
    // violations indicate that another process had the file open while we were
    // trying to rename. Anti-virus is believed to be the cause of this error in
    // the wild. Treated as a transient error on the assumption that the file
    // will be made available for renaming at a later time.
    case ERROR_SHARING_VIOLATION:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
      break;

    // This is also not a documented return value of SHFileOperation, but has
    // been observed in the wild. We are treating it as a transient error based
    // on the cases we have seen so far.  See http://crbug.com/368455.
    case ERROR_INVALID_PARAMETER:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
      break;

    // The source and destination files are the same file.
    // DE_SAMEFILE == 0x71
    case 0x71:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The operation was canceled by the user, or silently canceled if the
    // appropriate flags were supplied to SHFileOperation.
    // DE_OPCANCELLED == 0x75
    case 0x75:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // Security settings denied access to the source.
    // DE_ACCESSDENIEDSRC == 0x78
    case 0x78:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The source or destination path exceeded or would exceed MAX_PATH.
    // DE_PATHTOODEEP == 0x79
    case 0x79:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;
      break;

    // The path in the source or destination or both was invalid.
    // DE_INVALIDFILES == 0x7C
    case 0x7C:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The destination path is an existing file.
    // DE_FLDDESTISFILE == 0x7E
    case 0x7E:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The destination path is an existing folder.
    // DE_FILEDESTISFLD == 0x80
    case 0x80:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The name of the file exceeds MAX_PATH.
    // DE_FILENAMETOOLONG == 0x81
    case 0x81:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;
      break;

    // The destination is a read-only CD-ROM, possibly unformatted.
    // DE_DEST_IS_CDROM == 0x82
    case 0x82:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The destination is a read-only DVD, possibly unformatted.
    // DE_DEST_IS_DVD == 0x83
    case 0x83:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The destination is a writable CD-ROM, possibly unformatted.
    // DE_DEST_IS_CDRECORD == 0x84
    case 0x84:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The file involved in the operation is too large for the destination
    // media or file system.
    // DE_FILE_TOO_LARGE == 0x85
    case 0x85:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE;
      break;

    // The source is a read-only CD-ROM, possibly unformatted.
    // DE_SRC_IS_CDROM == 0x86
    case 0x86:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The source is a read-only DVD, possibly unformatted.
    // DE_SRC_IS_DVD == 0x87
    case 0x87:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // The source is a writable CD-ROM, possibly unformatted.
    // DE_SRC_IS_CDRECORD == 0x88
    case 0x88:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
      break;

    // MAX_PATH was exceeded during the operation.
    // DE_ERROR_MAX == 0xB7
    case 0xB7:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;
      break;

    // An unspecified error occurred on the destination.
    // XE_ERRORONDEST == 0x10000
    case 0x10000:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // Multiple file paths were specified in the source buffer, but only one
    // destination file path.
    // DE_MANYSRC1DEST == 0x72
    case 0x72:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // Rename operation was specified but the destination path is
    // a different directory. Use the move operation instead.
    // DE_DIFFDIR == 0x73
    case 0x73:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The source is a root directory, which cannot be moved or renamed.
    // DE_ROOTDIR == 0x74
    case 0x74:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The destination is a subtree of the source.
    // DE_DESTSUBTREE == 0x76
    case 0x76:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The operation involved multiple destination paths,
    // which can fail in the case of a move operation.
    // DE_MANYDEST == 0x7A
    case 0x7A:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // The source and destination have the same parent folder.
    // DE_DESTSAMETREE == 0x7D
    case 0x7D:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // An unknown error occurred.  This is typically due to an invalid path in
    // the source or destination.  This error does not occur on Windows Vista
    // and later.
    // DE_UNKNOWN_ERROR == 0x402
    case 0x402:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;

    // Destination is a root directory and cannot be renamed.
    // DE_ROOTDIR | ERRORONDEST == 0x10074
    case 0x10074:
      result = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
      break;
  }

  if (result != DOWNLOAD_INTERRUPT_REASON_NONE)
    return result;

  // If not one of the above codes, it should be a standard Windows error code.
  return ConvertFileErrorToInterruptReason(
      base::File::OSErrorToFileError(code));
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
												
  if(base::win::GetVersion() < base::win::Version::VISTA) {	  
	  // The parameters to SHFileOperation must be terminated with 2 NULL chars.
	  base::FilePath::StringType source = full_path_.value();
	  base::FilePath::StringType target = new_path.value();

	  source.append(1, L'\0');
	  target.append(1, L'\0');

	  SHFILEOPSTRUCT move_info = {nullptr};
	  move_info.wFunc = FO_MOVE;
	  move_info.pFrom = source.c_str();
	  move_info.pTo = target.c_str();
	  move_info.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI |
						 FOF_NOCONFIRMMKDIR | FOF_NOCOPYSECURITYATTRIBS;

	  int result = SHFileOperation(&move_info);
	  DownloadInterruptReason interrupt_reason = DOWNLOAD_INTERRUPT_REASON_NONE;

	  if (result == 0 && move_info.fAnyOperationsAborted)
		interrupt_reason = DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
	  else if (result != 0)
		interrupt_reason = MapShFileOperationCodes(result);

	  if (interrupt_reason != DOWNLOAD_INTERRUPT_REASON_NONE)
		return LogInterruptReason("SHFileOperation", result, interrupt_reason);	

	  return interrupt_reason;	
  }

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
