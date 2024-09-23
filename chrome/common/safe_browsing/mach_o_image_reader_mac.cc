// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"

#include <libkern/OSByteOrder.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"

namespace safe_browsing {

// ByteSlice is a bounds-checking view of an arbitrary byte array.
class ByteSlice {
 public:
  // Creates an invalid byte slice.
  ByteSlice() : ByteSlice(nullptr, 0) {}

  // Creates a slice for a given data array.
  explicit ByteSlice(const uint8_t* data, size_t size)
      : data_(data), size_(size) {}
  ~ByteSlice() {}

  bool IsValid() {
    return data_ != nullptr;
  }

  // Creates a sub-slice from the current slice.
  ByteSlice Slice(size_t at, size_t size) {
    if (!RangeCheck(at, size))
      return ByteSlice();
    return ByteSlice(data_ + at, size);
  }

  // Casts an offset to a specific type.
  template <typename T>
  const T* GetPointerAt(size_t at) {
    if (!RangeCheck(at, sizeof(T)))
      return nullptr;
    return reinterpret_cast<const T*>((data_ + at).get());
  }

  // Copies data from an offset to a buffer.
  bool CopyDataAt(size_t at, size_t size, uint8_t* out_data) {
    if (!RangeCheck(at, size))
      return false;
    memcpy(out_data, data_ + at, size);
    return true;
  }

  bool RangeCheck(size_t offset, size_t size) {
    if (offset >= size_)
      return false;
    base::CheckedNumeric<size_t> range(offset);
    range += size;
    if (!range.IsValid())
      return false;
    return range.ValueOrDie() <= size_;
  }

  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  raw_ptr<const uint8_t, AllowPtrArithmetic> data_;
  size_t size_;

  // Copy and assign allowed.
};

MachOImageReader::LoadCommand::LoadCommand() {}

MachOImageReader::LoadCommand::LoadCommand(const LoadCommand& other) = default;

MachOImageReader::LoadCommand::~LoadCommand() {}

// static
bool MachOImageReader::IsMachOMagicValue(uint32_t magic) {
  return magic == FAT_MAGIC   || magic == FAT_CIGAM    ||
         magic == MH_MAGIC    || magic == MH_CIGAM     ||
         magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

MachOImageReader::MachOImageReader()
    : data_(),
      is_fat_(false),
      is_64_bit_(false),
      commands_() {
}

MachOImageReader::~MachOImageReader() {}

bool MachOImageReader::Initialize(const uint8_t* image, size_t image_size) {
  if (!image)
    return false;

  data_ = std::make_unique<ByteSlice>(image, image_size);

  const uint32_t* magic = data_->GetPointerAt<uint32_t>(0);
  if (!magic)
    return false;

  // Check if this is a fat file. Note that the fat_header and fat_arch
  // structs are always in big endian.
  is_fat_ = *magic == FAT_MAGIC || *magic == FAT_CIGAM;
  if (is_fat_) {
    const fat_header* header = data_->GetPointerAt<fat_header>(0);
    if (!header)
      return false;

    bool do_swap = header->magic == FAT_CIGAM;
    uint32_t nfat_arch = do_swap ? OSSwapInt32(header->nfat_arch)
                                 : header->nfat_arch;

    size_t offset = sizeof(*header);
    for (uint32_t i = 0; i < nfat_arch; ++i) {
      const fat_arch* arch = data_->GetPointerAt<fat_arch>(offset);
      if (!arch)
        return false;

      uint32_t arch_offset = do_swap ? OSSwapInt32(arch->offset) : arch->offset;
      uint32_t arch_size = do_swap ? OSSwapInt32(arch->size) : arch->size;

      // Cannot refer back to headers of previous arches to cause
      // recursive processing.
      if (arch_offset < offset)
        return false;

      ByteSlice slice = data_->Slice(arch_offset, arch_size);
      if (!slice.IsValid())
        return false;

      fat_images_.push_back(std::make_unique<MachOImageReader>());
      if (!fat_images_.back()->Initialize(slice.data(), slice.size()))
        return false;

      offset += sizeof(*arch);
    }

    return true;
  }

  bool do_swap = *magic == MH_CIGAM || *magic == MH_CIGAM_64;

  // Make sure this is a Mach-O file.
  is_64_bit_ = *magic == MH_MAGIC_64 || *magic == MH_CIGAM_64;
  if (!(is_64_bit_ || *magic == MH_MAGIC || do_swap))
    return false;

  // Read the full Mach-O image header.
  if (is_64_bit_) {
    if (!GetMachHeader64())
      return false;
  } else {
    if (!GetMachHeader())
      return false;
  }

  // Collect all the load commands for the binary.
  const size_t load_command_size = sizeof(load_command);
  size_t offset = is_64_bit_ ? sizeof(mach_header_64) : sizeof(mach_header);
  const uint32_t num_commands = do_swap ? OSSwapInt32(GetMachHeader()->ncmds)
                                        : GetMachHeader()->ncmds;
  commands_.resize(num_commands);
  for (uint32_t i = 0; i < num_commands; ++i) {
    LoadCommand* command = &commands_[i];

    command->data.resize(load_command_size);
    if (!data_->CopyDataAt(offset, load_command_size, &command->data[0])) {
      return false;
    }

    uint32_t cmdsize = do_swap ? OSSwapInt32(command->cmdsize())
                               : command->cmdsize();
    // If the load_command's reported size is smaller than the size of the base
    // struct, do not try to copy additional data (or resize to be smaller
    // than the base struct). This may not be valid Mach-O.
    if (cmdsize < load_command_size) {
      offset += load_command_size;
      continue;
    }

    command->data.resize(cmdsize);
    if (!data_->CopyDataAt(offset, cmdsize, &command->data[0])) {
      return false;
    }

    offset += cmdsize;
  }

  return true;
}

bool MachOImageReader::IsFat() {
  return is_fat_;
}

std::vector<MachOImageReader*> MachOImageReader::GetFatImages() {
  DCHECK(is_fat_);
  std::vector<MachOImageReader*> images;
  for (const auto& image : fat_images_)
    images.push_back(image.get());
  return images;
}

bool MachOImageReader::Is64Bit() {
  DCHECK(!is_fat_);
  return is_64_bit_;
}

const mach_header* MachOImageReader::GetMachHeader() {
  DCHECK(!is_fat_);
  return data_->GetPointerAt<mach_header>(0);
}

const mach_header_64* MachOImageReader::GetMachHeader64() {
  DCHECK(is_64_bit_);
  DCHECK(!is_fat_);
  return data_->GetPointerAt<mach_header_64>(0);
}

uint32_t MachOImageReader::GetFileType() {
  DCHECK(!is_fat_);
  return GetMachHeader()->filetype;
}

const std::vector<MachOImageReader::LoadCommand>&
MachOImageReader::GetLoadCommands() {
  DCHECK(!is_fat_);
  return commands_;
}

bool MachOImageReader::GetCodeSignatureInfo(std::vector<uint8_t>* info) {
  DCHECK(!is_fat_);
  DCHECK(info->empty());

  // Find the LC_CODE_SIGNATURE command and cast it to its linkedit format.
  const linkedit_data_command* lc_code_signature = nullptr;
  for (const auto& command : commands_) {
    if (command.cmd() == LC_CODE_SIGNATURE) {
      lc_code_signature = command.as_command<linkedit_data_command>();
      break;
    }
  }
  if (lc_code_signature == nullptr)
    return false;

  info->resize(lc_code_signature->datasize);
  return data_->CopyDataAt(lc_code_signature->dataoff,
                           lc_code_signature->datasize,
                           &(*info)[0]);
}

}  // namespace safe_browsing
