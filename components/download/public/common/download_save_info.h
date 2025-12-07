// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/download/public/common/download_export.h"
#include "crypto/secure_hash.h"

namespace download {

// Invalid offset for http range request.
constexpr int64_t kInvalidRange = -1;

// Holds the information about how to save a download file.
// In the case of download continuation, |file_path| is set to the current file
// name, |offset| is set to the point where we left off, and |hash_state| will
// hold the state of the hash algorithm where we left off.
struct COMPONENTS_DOWNLOAD_EXPORT DownloadSaveInfo {
  // The default value for |length|. Used when request the rest of the file
  // starts from |offset|.
  static const int64_t kLengthFullContent;

  DownloadSaveInfo();

  DownloadSaveInfo(const DownloadSaveInfo&) = delete;
  DownloadSaveInfo& operator=(const DownloadSaveInfo&) = delete;

  DownloadSaveInfo(DownloadSaveInfo&& that);

  ~DownloadSaveInfo();

  int64_t GetStartingFileWriteOffset() const;

  bool IsArbitraryRangeRequest() const;

  // If non-empty, contains the full target path of the download that has been
  // determined prior to download initiation. This is considered to be a trusted
  // path.
  base::FilePath file_path;

  // If non-empty, contains an untrusted filename suggestion. This can't contain
  // a path (only a filename), and is only effective if |file_path| is empty.
  std::u16string suggested_name;

  // If valid, contains the source data stream for the file contents.
  base::File file;

  // Represents the offset for http range request header. e.g, "Range:
  // bytes=0-1023". |kInvalidRange| is used as initial value or open ended
  // range. e.g, |range_request_to| with |kInvalidRange| can result in the
  // following header:  "Range: bytes=100-". Notice this could be different than
  // |offset|.
  int64_t range_request_from = kInvalidRange;
  int64_t range_request_to = kInvalidRange;

  // The file offset to start receiving download data, could be different from
  // the network offset when |range_request_from| and |range_request_to| are
  // used. During resumption, |offset| could be smaller than the downloaded
  // content length. This is because download may request some data(from
  // |offset| to |file_offset|) to validate whether the content has changed.
  int64_t offset = 0;

  // The file offset to start writing to disk. If this value is negative,
  // download stream will be writing to the disk starting at |offset|.
  // Otherwise, this value will be used. Data received before |file_offset| are
  // used for validation purpose, and will not be written to disk.
  int64_t file_offset = -1;

  // The state of the hash. If specified, this hash state must indicate the
  // state of the partial file for the first |offset| bytes.
  std::unique_ptr<crypto::SecureHash> hash_state;

  // SHA-256 hash of the first |offset| bytes of the file. Only used if |offset|
  // is non-zero and either |file_path| or |file| specifies the file which
  // contains the |offset| number of bytes. Can be empty, in which case no
  // verification is done on the existing file.
  std::string hash_of_partial_file;

  // If |prompt_for_save_location| is true, and |file_path| is empty, then
  // the user will be prompted for a location to save the download. Otherwise,
  // the location will be determined automatically using |file_path| as a
  // basis if |file_path| is not empty.
  bool prompt_for_save_location = false;

  // Whether the file should be stored in memory.
  bool use_in_memory_file = false;

  // Whether the file contents need to be obfuscated.
  bool needs_obfuscation = false;

  // The size of the response body. If content-length response header is not
  // presented or can't be parse, set to 0.
  int64_t total_bytes = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_
