// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/gif_file_writer.h"
#include "base/containers/span.h"

namespace recording {

GifFileWriter::GifFileWriter(
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& gif_file_path,
    RecordingFileIoHelper::Delegate* file_io_helper_delegate)
    : gif_file_(gif_file_path,
                base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE),
      file_io_helper_(gif_file_path,
                      std::move(drive_fs_quota_delegate),
                      file_io_helper_delegate) {}

GifFileWriter::~GifFileWriter() = default;

void GifFileWriter::WriteByte(uint8_t byte) {
  WriteBytesAndCheck(base::make_span(&byte, sizeof(byte)));
}

void GifFileWriter::WriteBuffer(const uint8_t* const buffer,
                                size_t buffer_size) {
  WriteBytesAndCheck(base::make_span(buffer, buffer_size));
}

void GifFileWriter::WriteBytesAndCheck(base::span<const uint8_t> data) {
  if (!gif_file_.WriteAtCurrentPosAndCheck(data)) {
    file_io_helper_.delegate()->NotifyFailure(mojom::RecordingStatus::kIoError);
    return;
  }

  file_io_helper_.OnBytesWritten(data.size_bytes());
}

}  // namespace recording
