// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <windows.h>

#include <cguid.h>
#include <objbase.h>
#include <shellapi.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_stats.h"

namespace download {
namespace {

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

}  // namespace

// Renames a file using the SHFileOperation API to ensure that the target file
// gets the correct default security descriptor in the new path.
// Returns a network error, or net::OK for success.
DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

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

}  // namespace download
