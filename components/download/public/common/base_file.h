// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_BASE_FILE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_BASE_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace download {

// File being downloaded and saved to disk. This is a base class
// for DownloadFile and SaveFile, which keep more state information. BaseFile
// considers itself the owner of the physical file and will delete it when the
// BaseFile object is destroyed unless the ownership is revoked via a call to
// Detach().
class COMPONENTS_DOWNLOAD_EXPORT BaseFile {
 public:
  // May be constructed on any thread.  All other routines (including
  // destruction) must occur on the same sequence.
  BaseFile(uint32_t download_id);
  ~BaseFile();

  // Returns DOWNLOAD_INTERRUPT_REASON_NONE on success, or a
  // DownloadInterruptReason on failure. Upon success, the file at |full_path()|
  // is assumed to be owned by the BaseFile. It will be deleted when the
  // BaseFile object is destroyed unless Detach() is called before destroying
  // the BaseFile instance.
  //
  // |full_path|: Full path to the download file. Can be empty, in which case
  //     the rules described in |default_directory| will be used to generate a
  //     temporary filename.
  //
  // |default_directory|: specifies the directory to create the temporary file
  //     in if |full_path| is empty. If |default_directory| and |full_path| are
  //     empty, then a temporary file will be created in the default download
  //     location as determined by ContentBrowserClient.
  //
  // |file|: The base::File handle to use. If specified, BaseFile will not open
  //     a file and will use this handle. The file should be opened for both
  //     read and write. Only makes sense if |full_path| is non-empty since it
  //     implies that the caller already knows the path to the file.  There's no
  //     perfect way to come up with a canonical path for a file. So BaseFile
  //     will not attempt to determine the |full_path|.
  //
  // |bytes_so_far|: If a file is provided (via |full_path| or |file|), then
  //     this argument specifies the amount of data that has been written to
  //     the file. If |is_sparse_file| is false, this value should be the size
  //     of the file to expect. It is legal for the file to be larger, in which
  //     case the file will be truncated down to this size if |is_sparse_file|
  //     is false. However, if the file is shorter, then the operation will
  //     fail with DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT.

  //
  // |hash_so_far|: If |bytes_so_far| is non-zero and |is_sparse_file| is
  //     false, this specifies the SHA-256 hash of the first |bytes_so_far|
  //     bytes of the target file. If specified, BaseFile will read the first
  //     |bytes_so_far| of the target file in order to calculate the hash and
  //     verify that the file matches. If there's a mismatch, then the operation
  //     fails with DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH. Not used if
  //     |hash_state| is also specified.
  //
  // |hash_state|: The partial hash object to use. Only meaningful if there's a
  //     preexisting target file and it is non-empty (i.e. bytes_so_far is
  //     non-zero). If specified, BaseFile will assume that the bytes up to
  //     |bytes_so_far| has been accurately hashed into |hash_state| and will
  //     ignore |hash_so_far|. Not used if |is_sparse_file| is true.
  //
  // |is_sparse_file|: Specifies whether the file is a sparse file. If so, it is
  //     possible that a write can happen at an offset that is larger than the
  //     file size, thus creating holes in it.
  //
  // |bytes_wasted|: Address of an integer that will handle bytes_wasted if
  //     there is a significant amount (ie. greater than 0).
  DownloadInterruptReason Initialize(
      const base::FilePath& full_path,
      const base::FilePath& default_directory,
      base::File file,
      int64_t bytes_so_far,
      const std::string& hash_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state,
      bool is_sparse_file,
      int64_t* const bytes_wasted);

  // Write a new chunk of data to the file. Returns a DownloadInterruptReason
  // indicating the result of the operation. Works only if |is_sparse_file| is
  // false.
  DownloadInterruptReason AppendDataToFile(const char* data, size_t data_len);

  // Write a new chunk of data to the file. Returns a DownloadInterruptReason
  // indicating the result of the operation.
  DownloadInterruptReason WriteDataToFile(int64_t offset,
                                          const char* data,
                                          size_t data_len);

  // Validates that the content starting from |offset| matches that of |data|
  // with the given length.
  bool ValidateDataInFile(int64_t offset, const char* data, size_t data_len);

  // Rename the download file. Returns a DownloadInterruptReason indicating the
  // result of the operation. A return code of NONE indicates that the rename
  // was successful. After a failure, the full_path() and in_progress() can be
  // used to determine the last known filename and whether the file is available
  // for writing or retrying the rename. Call Finish() to obtain the last known
  // hash state.
  DownloadInterruptReason Rename(const base::FilePath& full_path);

  // Mark the file as detached. Up until this method is called, BaseFile assumes
  // ownership of the file and hence will delete the file if the BaseFile object
  // is destroyed. Calling Detach() causes BaseFile to assume that it no longer
  // owns the file. Detach() can be called at any time. Close() must still be
  // called to close the file if it is open.
  void Detach();

  // Abort the download and automatically close and delete the file.
  void Cancel();

  // Indicate that the download has finished. No new data will be received.
  // Returns the SecureHash object representing the state of the hash function
  // at the end of the operation. If |is_sparse_file_| is true, calling this
  // will cause |secure_hash_| to get calculated.
  std::unique_ptr<crypto::SecureHash> Finish();

  // Informs the OS that this file came from the internet. Returns a
  // DownloadInterruptReason indicating the result of the operation.
  //
  // |client_guid|: The client GUID which will be used to identify the caller to
  //     the system AV scanning function.
  //
  // |source_url| / |referrer_url|: Source and referrer for the network request
  //     that originated this download. Will be used to annotate source
  //     information and also to determine the relative danger level of the
  //     file.
  DownloadInterruptReason AnnotateWithSourceInformationSync(
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url);

  // Callback used with AnnotateWithSourceInformation.
  // Created by DownloadFileImpl::RenameWithRetryInternal
  // to bind DownloadFileImpl::OnRenameComplete.
  using OnAnnotationDoneCallback =
      base::OnceCallback<void(DownloadInterruptReason)>;

  // Called when a quarantine service is used.
  // and the callback will be called from the service.
  // TODO (crbug.com/973497): Remove non-service version when
  // kPreventDownloadsWithSamePath feature is removed.
  void AnnotateWithSourceInformation(
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      OnAnnotationDoneCallback on_annotation_done_callback);

#if defined(OS_ANDROID)
  // Publishes the intermediate download to public download collection.
  DownloadInterruptReason PublishDownload();
#endif

  // Returns the last known path to the download file. Can be empty if there's
  // no file.
  const base::FilePath& full_path() const { return full_path_; }

  // Returns true if the file is open. If true, the file can be written to or
  // renamed.
  bool in_progress() const { return file_.IsValid(); }

  // Returns the number of bytes that has been written so far. If
  // |is_sparse_file_| is false, this should always be equal to the file size.
  // If |is_sparse_file_| is true, this should not be larger than the file size
  // as the file may contain holes in it.
  int64_t bytes_so_far() const { return bytes_so_far_; }

  std::string DebugString() const;

 private:
  friend class BaseFileTest;
  FRIEND_TEST_ALL_PREFIXES(BaseFileTest, IsEmptyHash);

  // Creates and opens the file_ if it is invalid.
  //
  // If |is_sparse_file_| is false and |hash_so_far| is not empty, then it must
  // match the SHA-256 hash of the first |bytes_so_far_| bytes of |file_|. If
  // there's a hash mismatch, Open() fails with
  // DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH.
  //
  // If the opened file is shorter than |bytes_so_far_| bytes, then Open() fails
  // with DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT. If the opened file is longer
  // and |is_sparse_file_| is false, then the file is truncated to
  // |bytes_so_far_|.
  //
  // Open() can fail for other reasons as well. In that case, it returns a
  // relevant interrupt reason. Unless Open() return
  // DOWNLOAD_INTERRUPT_REASON_NONE, it should be assumed that |file_| is not
  // valid.
  DownloadInterruptReason Open(const std::string& hash_so_far,
                               int64_t* const bytes_wasted);

  // Closes and resets file_.
  void Close();

  // Resets file_.
  void ClearFile();

  // Platform specific method that moves a file to a new path and adjusts the
  // security descriptor / permissions on the file to match the defaults for the
  // new directory.
  DownloadInterruptReason MoveFileAndAdjustPermissions(
      const base::FilePath& new_path);

  // Split out from CurrentSpeed to enable testing.
  int64_t CurrentSpeedAtTime(base::TimeTicks current_time) const;

  // Verifies that:
  // * Size of the file represented by |file_| is at least |bytes_so_far_|.
  //
  // * If |hash_to_expect| is not empty, then the result of hashing the first
  //   |bytes_so_far_| bytes of |file_| matches |hash_to_expect|.
  //
  // If the result is REASON_NONE, then on return |secure_hash_| is valid and
  // is ready to hash bytes from offset |bytes_so_far_| + 1.
  // If |is_sparse_file_| is true, this function is only called when Finish()
  // is called.
  DownloadInterruptReason CalculatePartialHash(
      const std::string& hash_to_expect);

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |error| and passes error
  // on through, converting to a |DownloadInterruptReason|.
  DownloadInterruptReason LogNetError(const char* operation, net::Error error);

  // Log the system error in |os_error| and converts it into a
  // |DownloadInterruptReason|.
  DownloadInterruptReason LogSystemError(const char* operation,
                                         logging::SystemErrorCode os_error);

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |os_error| and |reason|.
  // Returns |reason|.
  DownloadInterruptReason LogInterruptReason(const char* operation,
                                             int os_error,
                                             DownloadInterruptReason reason);

  // Callback invoked when quarantine service has an error.
  void OnQuarantineServiceError(const GURL& source_url,
                                const GURL& referrer_url);

  // Callback invoked by quarantine service. Also called by
  // OnQuarantineServiceError after manually applying mark-of-the-web.
  void OnFileQuarantined(bool connection_error,
                         quarantine::mojom::QuarantineFileResult result);

  // Full path to the file including the file name.
  base::FilePath full_path_;

  // OS file for writing
  base::File file_;

  // Amount of data received up so far, in bytes.
  int64_t bytes_so_far_ = 0;

  // Used to calculate hash for the file when calculate_hash_ is set.
  std::unique_ptr<crypto::SecureHash> secure_hash_;

  // Start time for calculating speed.
  base::TimeTicks start_tick_;

  // Indicates that this class no longer owns the associated file, and so
  // won't delete it on destruction.
  bool detached_ = false;

  // Whether the file is sparse.
  // TODO(qinmin): pass the slice information to this class so that we can
  // verify that writes are not overlapping.
  bool is_sparse_file_ = false;

  // ID of the download, used for trace events.
  uint32_t download_id_;

  // Mojo remote for quarantine service.
  mojo::Remote<quarantine::mojom::Quarantine> quarantine_service_;

  // Callback invoked after quarantine service finishes.
  OnAnnotationDoneCallback on_annotation_done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BaseFile> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BaseFile);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_BASE_FILE_H_
