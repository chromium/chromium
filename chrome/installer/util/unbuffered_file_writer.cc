// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/unbuffered_file_writer.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/drive_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/process/memory.h"
#include "base/types/expected_macros.h"

namespace installer {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Operation {
  kCreate = 0,
  // kGetInfo = 1,
  kAllocate = 2,
  kWrite = 3,
  kSetInfo = 4,
  kClearDelete = 5,
  kMaxValue = kClearDelete,
};

std::string_view ToString(Operation operation) {
  switch (operation) {
    case Operation::kCreate:
      return "Create";
    case Operation::kAllocate:
      return "Allocate";
    case Operation::kWrite:
      return "Write";
    case Operation::kSetInfo:
      return "SetInfo";
    case Operation::kClearDelete:
      return "ClearDelete";
  }
}

void RecordFailure(Operation operation, DWORD error_code) {
  base::UmaHistogramSparse(base::StrCat({"Setup.Install.UnbufferedFileWriter.",
                                         ToString(operation), ".Error"}),
                           static_cast<int>(error_code));
}

// Returns `value` rounded up to be an integral number of `chunk_size` chunks.
size_t RoundUp(size_t value, size_t chunk_size) {
  if (auto remainder = base::CheckMod(value, chunk_size);
      remainder.ValueOrDie() == 0) {
    return value;
  } else {
    return base::CheckAdd(value,
                          base::CheckSub(chunk_size, std::move(remainder)))
        .ValueOrDie();
  }
}

// Returns `value` rounded down to be an integral number of `chunk_size` chunks.
size_t RoundDown(size_t value, size_t chunk_size) {
  return base::CheckSub(value, base::CheckMod(value, chunk_size)).ValueOrDie();
}

// A deleter function that frees memory allocated via VirtualAlloc.
void VirtualFreeDeleteFn(void* ptr) {
  PCHECK(::VirtualFree(ptr, /*dwSize=*/0, MEM_RELEASE));
}

// Returns an empty AlignedBuffer.
base::HeapArray<uint8_t, void (*)(void*)> MakeEmptyBuffer() {
  // SAFETY: No allocation; deleter will never be called.
  return UNSAFE_BUFFERS(
      base::HeapArray<uint8_t, void (*)(void*)>::FromOwningPointer(nullptr, 0,
                                                                   nullptr));
}

// Returns the physical sector size for the device on which `path` resides,
// falling back to 4096 bytes (the norm as of this writing) if the value cannot
// be determined.
DWORD DeterminePhysicalSectorSize(const base::FilePath& path) {
  // TODO(crbug.com/456155453): Remove UMA and logging from this function once
  // this has been verified on stable.

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    kSuccess = 0,
    kDriveInfoFailed = 1,
    kNoSizeValue = 2,
    kGreaterThanFourK = 3,
    kNotPowerOfTwo = 4,
    kMaxValue = kNotPowerOfTwo
  };

  Result result = Result::kSuccess;  // Be optimistic.
  std::optional<DWORD> reported_sector_size;
  DWORD sector_size = 4096;  // A safe default in case all else fails.

  if (auto drive_info = base::GetFileDriveInfo(path); !drive_info.has_value()) {
    result = Result::kDriveInfoFailed;
  } else if (!drive_info->bytes_per_sector.has_value()) {
    result = Result::kNoSizeValue;
  } else {
    reported_sector_size = *drive_info->bytes_per_sector;
    if (!std::has_single_bit(*reported_sector_size)) {
      result = Result::kNotPowerOfTwo;
    } else if (*reported_sector_size > 4096) {
      result = Result::kGreaterThanFourK;
    } else {
      sector_size = *drive_info->bytes_per_sector;
    }
  }

  // Emit histograms and log only once per process.
  [[maybe_unused]] static const bool have_logged_once =
      [](Result result, std::optional<DWORD> reported_sector_size) {
        base::UmaHistogramEnumeration(
            "Setup.Install.UnbufferedFileWriter."
            "DeterminePhysicalSectorSizeResult",
            result);
        if (reported_sector_size.has_value()) {
          base::UmaHistogramSparse(
              "Setup.Install.UnbufferedFileWriter.PhysicalSectorSize",
              *reported_sector_size);
        }
        switch (result) {
          case Result::kSuccess:
            break;
          case Result::kDriveInfoFailed:
            PLOG(ERROR)
                << "Failed to get drive info; assuming 4KiB sector size.";
            break;
          case Result::kNoSizeValue:
            LOG(ERROR) << "Failed to determine physical sector size; assuming"
                          " 4KiB.";
            break;
          case Result::kGreaterThanFourK:
            LOG(ERROR) << "Device reported " << *reported_sector_size
                       << " bytes per sector, which is highly unusual; trying"
                          " with 4KiB.";
            break;
          case Result::kNotPowerOfTwo:
            LOG(ERROR) << "Device reported " << *reported_sector_size
                       << " bytes per sector, which is not a power of two; "
                          "trying with 4KiB.";
            break;
        }
        return true;
      }(result, reported_sector_size);

  return sector_size;
}

}  // namespace

// static
base::expected<UnbufferedFileWriter, DWORD> UnbufferedFileWriter::Create(
    const base::FilePath& path,
    int64_t buffer_size) {
  CHECK(!path.empty());
  CHECK_GE(buffer_size, 0LL);

  base::File file(::CreateFileW(path.value().c_str(),
                                /*dwDesiredAccess=*/GENERIC_WRITE | DELETE,
                                /*dwShareMode=*/FILE_SHARE_DELETE,
                                /*lpSecurityAttributes=*/nullptr,
                                /*dwCreationDisposition=*/CREATE_NEW,
                                /*dwFlagsAndAttributes=*/FILE_ATTRIBUTE_NORMAL |
                                    FILE_FLAG_NO_BUFFERING |
                                    FILE_FLAG_WRITE_THROUGH,
                                /*hTemplateFile=*/nullptr));

  if (!file.IsValid()) {
    auto error = ::GetLastError();
    PLOG(ERROR) << "CreateFileW failed";
    RecordFailure(Operation::kCreate, error);
    return base::unexpected(error);
  }

  // The file is deleted unless committed.
  file.DeleteOnClose(true);

  const DWORD physical_sector_size = DeterminePhysicalSectorSize(path);

  // Make the buffer size an even multiple of the physical sector size.
  buffer_size = buffer_size ? RoundUp(buffer_size, physical_sector_size)
                            : physical_sector_size;

  // Allocate a properly-aligned write buffer.
  AlignedBuffer buffer = AllocateAligned(buffer_size, physical_sector_size);
  if (buffer_size && buffer.empty()) {
    auto error = ::GetLastError();
    PLOG(ERROR) << "Allocation failed";
    RecordFailure(Operation::kAllocate, error);
    return base::unexpected(error);
  }

  return UnbufferedFileWriter(std::move(file), physical_sector_size,
                              std::move(buffer));
}

UnbufferedFileWriter::UnbufferedFileWriter(UnbufferedFileWriter&&) = default;
UnbufferedFileWriter& UnbufferedFileWriter::operator=(UnbufferedFileWriter&&) =
    default;

UnbufferedFileWriter::~UnbufferedFileWriter() = default;

void UnbufferedFileWriter::Advance(size_t offset) {
  CHECK_LE(offset, buffer_.size() - data_size_);
  data_size_ += offset;
}

base::expected<void, DWORD> UnbufferedFileWriter::Checkpoint() {
  CHECK(file_.IsValid());  // Use-after-move or after Commit.

  RETURN_IF_ERROR(Write(/*include_final_incomplete_sector=*/false));
  return base::ok();
}

base::expected<void, DWORD> UnbufferedFileWriter::Commit(
    std::optional<base::Time> last_modified_time) {
  CHECK(file_.IsValid());  // Use-after-move or after Commit.

  ASSIGN_OR_RETURN(int64_t file_size,
                   Write(/*include_final_incomplete_sector=*/true));

  if ((file_size % physical_sector_size_) != 0) {
    // Truncate down to the actual size.
    FILE_END_OF_FILE_INFO information = {};
    information.EndOfFile.QuadPart = file_size;
    if (!::SetFileInformationByHandle(
            file_.GetPlatformFile(),
            /*FileInformationClass=*/FileEndOfFileInfo,
            /*lpFileInformation=*/&information,
            /*dwBufferSize=*/sizeof(information))) {
      auto error = ::GetLastError();
      PLOG(ERROR) << "SetFileInformationByHandle failed";
      RecordFailure(Operation::kSetInfo, error);
      return base::unexpected(error);
    }
  }

  // The file has been written and shrunk down to the correct size. Clear the
  // delete-on-close bit so that it is retained when closed below.
  if (!file_.DeleteOnClose(false)) {
    auto error = ::GetLastError();
    PLOG(ERROR) << "DeleteOnClose failed";
    RecordFailure(Operation::kClearDelete, error);
    return base::unexpected(error);
  }

  if (last_modified_time) {
    // Make a best-effort attempt to set the file time before closing the file.
    FILETIME filetime = last_modified_time->ToFileTime();
    ::SetFileTime(file_.GetPlatformFile(), /*lpCreationTime=*/nullptr,
                  /*lpLastAccessTime=*/nullptr, /*lpLastWriteTime=*/&filetime);
  }

  // Close the file now that everything has succeeded.
  file_.Close();

  return base::ok();
}

UnbufferedFileWriter::UnbufferedFileWriter(base::File file,
                                           DWORD physical_sector_size,
                                           AlignedBuffer buffer)
    : file_(std::move(file)),
      physical_sector_size_(physical_sector_size),
      buffer_(std::move(buffer)) {}

base::expected<int64_t, DWORD> UnbufferedFileWriter::Write(
    bool include_final_incomplete_sector) {
  if (!buffer_.size()) {
    // A previous call to Write(true) has succeeded. One may not call
    // Write(false) again in this case.
    CHECK(include_final_incomplete_sector);
    return base::ok(file_position_);
  }

  // The number of bytes available to write.
  const size_t unwritten_size = data_size_ - written_size_;

  // The number of bytes available to write including padding to bring it up to
  // an integral number of physical sectors.
  const size_t unwritten_size_padded =
      RoundUp(unwritten_size, physical_sector_size_);

  // The number of bytes for this write; including or not including the padding
  // to fill a complete sector at the end.
  const size_t to_write_size =
      include_final_incomplete_sector
          ? unwritten_size_padded
          : RoundDown(unwritten_size, physical_sector_size_);

  // The aligned data to write in this call and the unwritten data at the end of
  // the buffer that will not be written if `include_final_incomplete_sector` is
  // false.
  auto [to_write, after_write] =
      buffer_.subspan(written_size_, unwritten_size_padded)
          .split_at(include_final_incomplete_sector
                        ? unwritten_size_padded
                        : unwritten_size_padded -
                              (unwritten_size - to_write_size));

  // The data in the incomplete final sector that will not be written.
  if (include_final_incomplete_sector) {
    // Zero-pad the end of the buffer to the physical sector boundary.
    std::ranges::fill(to_write.last(unwritten_size_padded - unwritten_size), 0);
  }

  // Write the data to disk in chunks no larger than 2^31-1 bytes (the max
  // supported by base::File::Write).
  const size_t max_write_size =
      RoundDown(static_cast<size_t>(std::numeric_limits<int>::max()),
                physical_sector_size_);
  while (!to_write.empty()) {
    size_t this_write_size = std::min(max_write_size, to_write.size());
    base::span<uint8_t> this_write;
    std::tie(this_write, to_write) = to_write.split_at(this_write_size);
    if (file_.Write(file_position_, this_write) != this_write_size) {
      auto error = ::GetLastError();
      PLOG(ERROR) << "Write failed";
      RecordFailure(Operation::kWrite, error);
      return base::unexpected(error);
    }
    written_size_ += this_write_size;
    file_position_ += this_write_size;
  }

  // No written data remains in the buffer.
  written_size_ = 0;

  if (include_final_incomplete_sector) {
    // All data has been written. Release the buffer.
    buffer_ = MakeEmptyBuffer();
    data_size_ = 0;

    // Subtract the padding from the file position so that it now represents
    // the desired size of the file.
    file_position_ -= (unwritten_size_padded - unwritten_size);
  } else {
    // `after_write` holds data beyond the last complete sector that was
    // written. Shift this to the front of the buffer for the next round of
    // writes.
    buffer_.copy_prefix_from(after_write);
    data_size_ = after_write.size();
  }

  return base::ok(file_position_);
}

// static
UnbufferedFileWriter::AlignedBuffer UnbufferedFileWriter::AllocateAligned(
    size_t size,
    DWORD alignment) {
  // Attempt using the normal allocator. In non-debug builds, PartitionAlloc
  // will make sensible choices to do a mapped-allocation for large sizes.
  uint8_t* mem = nullptr;
  if (base::UncheckedAlignedAlloc(size, alignment,
                                  reinterpret_cast<void**>(&mem))) {
    // SAFETY: UncheckedAlignedAlloc allocates at least `size` bytes.
    return UNSAFE_BUFFERS(AlignedBuffer::FromOwningPointer(
        mem, size, &base::UncheckedAlignedFree));
  }

  // If the normal allocator failed, try making a pagefile-backed allocation.
  // This is necessary for very large allocations, as PartitionAlloc rejects
  // them outright.
  static const DWORD kGranularity = [] {
    SYSTEM_INFO system_info = {};
    ::GetSystemInfo(&system_info);
    return system_info.dwAllocationGranularity;
  }();

  // Add support for devices that have physical sector sizes greater than the
  // allocation granularity if needed (doubtful -- famous last words!).
  CHECK_EQ(kGranularity % alignment, 0U);
  mem = static_cast<uint8_t*>(::VirtualAlloc(
      /*lpAddress=*/0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (mem) {
    // SAFETY: VirtualAlloc allocates at least `size` bytes.
    return UNSAFE_BUFFERS(
        AlignedBuffer::FromOwningPointer(mem, size, &VirtualFreeDeleteFn));
  }
  return MakeEmptyBuffer();
}

}  // namespace installer
