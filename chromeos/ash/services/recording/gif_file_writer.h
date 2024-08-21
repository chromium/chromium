// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_GIF_FILE_WRITER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_GIF_FILE_WRITER_H_

#include <cstdint>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "chromeos/ash/services/recording/recording_file_io_helper.h"

namespace recording {

// Creates a file at the given `gif_file_path`, and provides various APIs to
// allow writing bytes to it with various lengths. It also takes care of
// handling any IO errors while writing as well as running out of disk space or
// DriveFS quota conditions.
class GifFileWriter {
 public:
  GifFileWriter(
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& gif_file_path,
      RecordingFileIoHelper::Delegate* file_io_helper_delegate);
  GifFileWriter(const GifFileWriter&) = delete;
  GifFileWriter& operator=(const GifFileWriter&) = delete;
  ~GifFileWriter();

  // Writes the given `bytes` to the `gif_file_`.
  void WriteByte(uint8_t byte);

  // Writes the contents of the given `buffer` to the `gif_file_`.
  void WriteBuffer(base::span<const uint8_t> buffer);

  // Writes the given `string` to the `gif_file_`.
  void WriteString(std::string_view string);

  // Writes the given 16-bit `value` to the `gif_file_` in little endian format
  // such that the least significant bit gets written first.
  void WriteShort(uint16_t value);

  // Flushes the contents of the file to the underlying storage.
  void FlushFile();

 private:
  // Writes the given `data` to the `gif_file_` and check for IO errors or disk
  // space / DriveFS quota issues.
  void WriteBytesAndCheck(base::span<const uint8_t> data);

  // The file created at `gif_file_path` to which the output of the GIF encoder
  // will be written.
  base::File gif_file_;

  // A helper that will be used to calculate either the remaining disk space (if
  // writing to a local file), or the remaining quota if the file exists on
  // DriveFS.
  RecordingFileIoHelper file_io_helper_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_GIF_FILE_WRITER_H_
