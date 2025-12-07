// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_UNBUFFERED_FILE_WRITER_H_
#define CHROME_INSTALLER_UTIL_UNBUFFERED_FILE_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

namespace base {
class FilePath;
}

namespace installer {

// Writes a file to disk using direct, unbuffered I/O.
//
// Considerations when using:
//
// A single allocation is made for the write buffer. An instance will try very
// hard to satisfy this request and will fail gracefully rather than crash if it
// can't be satisfied.
//
// Data written to disk does not go through the system cache. This is especially
// suitable for background writes that should not interfere with other uses of
// the system.
//
// Explicit flushes are not needed. Once `Commit` succeeds, the data has reached
// the disk and the disk has been told to put it to the physical media. There is
// no guarantee that the disk will obey this, but the same can be said of
// FlushFileBuffers.
//
// Checkpoint() and Commit() may fail with media errors. Both are safe to be
// called repeatedly on error to retry. Note that no experimentation has yet
// been done in this area to determine whether such retries will be fruitful or
// not.
//
// Destruction of an instance without a successful Commit() will result in
// deletion of the file.
//
// See
// https://learn.microsoft.com/windows/win32/FileIO/file-buffering and
// https://learn.microsoft.com/windows/win32/api/FileAPI/nf-fileapi-createfilea#caching-behavior
// for details about unbuffered, direct I/O on Windows.
//
// Sample usage with a single small write:
//
//   // Create a writer.
//   ASSIGN_OR_RETURN(auto writer, UnbufferedFileWriter::Create(path));
//   // Put some data in its write buffer.
//   size_t data_size = PutDataInBuffer(writer.write_buffer());
//   // Tell the writer how much of the buffer has been populated.
//   writer.Advance(data_size);
//   // Commit all data to disk.
//   RETURN_IF_ERROR(writer.Commit());
//
// Advanced usage w/ Checkpoint:
//
//   // Create a writer with a buffer to hold at least 10KiB.
//   ASSIGN_OR_RETURN(auto writer,
//                    UnbufferedFileWriter::Create(path, 10 * 1024));
//   // Emit and commit until all data has been written.
//   while (HaveDataToWrite()) {
//     size_t data_size = PutDataInBuffer(writer.write_buffer());
//     // Tell the writer how much of the buffer has been populated.
//     writer.Advance(data_size);
//     // Checkpoint to move data from the buffer to disk.
//     RETURN_IF_ERROR(writer.Checkpoint());
//   }
//   // Commit all data to disk.
//   RETURN_IF_ERROR(writer.Commit());
//
class UnbufferedFileWriter {
 public:
  // Creates and opens the file at `path`. `buffer_size`, if specified,
  // indicates the desired size of the buffer to hold data between checkpoints.
  // If none is specified, the buffer will be the size of a single physical
  // sector. The file will be deleted if `Commit()` is never called (including
  // in case of process termination). Returns a Windows system error code in
  // case of failure.
  static base::expected<UnbufferedFileWriter, DWORD> Create(
      const base::FilePath& path,
      int64_t buffer_size = 0);

  UnbufferedFileWriter(const UnbufferedFileWriter&) = delete;
  UnbufferedFileWriter& operator=(const UnbufferedFileWriter&) = delete;
  UnbufferedFileWriter(UnbufferedFileWriter&&);
  UnbufferedFileWriter& operator=(UnbufferedFileWriter&&);
  ~UnbufferedFileWriter();

  // A view into a buffer into which the caller must place data to write to the
  // file. At creation, this is at least as big as the `buffer_size` provided at
  // construction. If no `buffer_size` was provided, this will be precisely the
  // size of one physical sector of the disk to which the file is being written.
  // `Advance()` must be called before `Checkpoint()` or `Commit()` to indicate
  // how much of this buffer has ben populated with data. Any other use of the
  // instance invalidates any previously-obtained view to the buffer.
  base::span<uint8_t> write_buffer() { return buffer_.subspan(data_size_); }

  // Indicates that the first `offset` bytes of `write_buffer()` have been
  // populated and are ready to be written. The next call to `write_buffer()`
  // will return a correspondingly smaller view into the buffer. The next call
  // to `Checkpoint()` will write as many integral physical sectors from the
  // buffer to disk as possible. Note well: if `buffer_size` was not specified
  // at construction or is smaller than one physical sector, `Checkpoint()` will
  // have no effect. Such data will be written to disk only on `Commit()`.
  void Advance(size_t offset);

  // Writes as many complete physical sectors from the buffer to disk as
  // possible. Does nothing if `Advance()` has not been called to move forward
  // at least one full sector. Returns a Windows system error code in case of
  // failure. May be called repeatedly on failure to retry.
  base::expected<void, DWORD> Checkpoint();

  // Commits the file to disk; optionally setting the file's last-modified time.
  // Returns a Windows system error code in case of failure. May be called
  // repeatedly on failure to retry. The file is closed on success, in which
  // case the instance may not be used further.
  base::expected<void, DWORD> Commit(
      std::optional<base::Time> last_modified_time);

 private:
  using AlignedBuffer = base::HeapArray<uint8_t, void (*)(void*)>;

  UnbufferedFileWriter(base::File file,
                       DWORD physical_sector_size,
                       AlignedBuffer buffer);

  // Writes complete sectors from the contents of `buffer_` starting from
  // `written_size_` to the file starting at `file_position_`. If
  // `include_final_incomplete_sector` is true, the buffer's full contents up
  // through `data_size_` are written after padding with zeros produces a
  // complete physical sector at the end of the file. Otherwise, the data for
  // the final incomplete sector is moved to the front of `buffer_` to be
  // included in a subsequent write of at least one full sector. Returns the
  // size of the file, omitting any zero padding added for the final write.
  // Returns a Windows system error code in case of failure. May be called
  // repeatedly on failure to retry.
  base::expected<int64_t, DWORD> Write(bool include_final_incomplete_sector);

  // Returns an uninitialized buffer of at least `size` bytes aligned as
  // requested, or an empty buffer on failure to allocate.
  static AlignedBuffer AllocateAligned(size_t size, DWORD alignment);

  // The file being written; valid from construction through successful Commit.
  base::File file_;

  // The size of a physical sector of the disk on which `file_` resides.
  DWORD physical_sector_size_;

  // A buffer aligned to physical_sector_size_.
  AlignedBuffer buffer_;

  // The amount of data in `buffer_` that has not been written to disk.
  size_t data_size_ = 0;

  // The amount of data in `buffer_` that has been written to disk. This is
  // always strictly less than `data_size_` when non-zero. It is only non-zero
  // after a failed call to `Checkpoint()`.
  size_t written_size_ = 0;

  // The offset into the file where the last write completed.
  int64_t file_position_ = 0;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_UNBUFFERED_FILE_WRITER_H_
