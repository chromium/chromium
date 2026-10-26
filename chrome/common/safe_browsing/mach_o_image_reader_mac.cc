// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"

#include <libkern/OSByteOrder.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_math.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {

// ByteSlice is a bounds-checking view of an arbitrary byte array.
class ByteSlice {
 public:
  // Creates an invalid byte slice.
  ByteSlice() = default;

  // Creates a slice for a given data array.
  explicit ByteSlice(base::span<const uint8_t> data) : data_(data) {}
  ~ByteSlice() = default;

  bool IsValid() { return !data_.empty(); }

  // Creates a sub-slice from the current slice.
  ByteSlice Slice(size_t at, size_t size) {
    if (!RangeCheck(at, size)) {
      return ByteSlice();
    }
    return ByteSlice(data_.subspan(at, size));
  }

  // Casts an offset to a specific type.
  template <typename T>
  const T* GetPointerAt(size_t at) {
    if (!RangeCheck(at, sizeof(T))) {
      return nullptr;
    }
    return reinterpret_cast<const T*>(data_.subspan(at).data());
  }

  // Copies data from an offset to a buffer.
  bool CopyDataAt(size_t at, base::span<uint8_t> out_data) {
    if (!RangeCheck(at, out_data.size())) {
      return false;
    }
    out_data.copy_from(data_.subspan(at, out_data.size()));
    return true;
  }

  bool RangeCheck(size_t offset, size_t size) {
    if (offset >= data_.size()) {
      return false;
    }
    base::CheckedNumeric<size_t> range(offset);
    range += size;
    if (!range.IsValid()) {
      return false;
    }
    return range.ValueOrDie() <= data_.size();
  }

  base::span<const uint8_t> data() const { return data_; }

 private:
  base::raw_span<const uint8_t> data_;

  // Copy and assign allowed.
};

MachOImageReader::LoadCommand::LoadCommand() = default;

MachOImageReader::LoadCommand::LoadCommand(const LoadCommand& other) = default;

MachOImageReader::LoadCommand::~LoadCommand() = default;

// static
bool MachOImageReader::IsMachOMagicValue(uint32_t magic) {
  if (base::FeatureList::IsEnabled(kSafeBrowsingRustDmgParser)) {
    return dmg::rust_ffi::is_mach_o_magic_value(magic);
  }
  return magic == FAT_MAGIC   || magic == FAT_CIGAM    ||
         magic == MH_MAGIC    || magic == MH_CIGAM     ||
         magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

MachOImageReader::MachOImageReader() = default;

MachOImageReader::MachOImageReader(
    ::rust::Box<dmg::rust_ffi::RustMachOImageReader> rust_reader)
    : rust_reader_(std::move(rust_reader)) {}

MachOImageReader::~MachOImageReader() = default;

bool MachOImageReader::Initialize(base::span<const uint8_t> image) {
  if (base::FeatureList::IsEnabled(kSafeBrowsingRustDmgParser)) {
    fat_images_.clear();
    rust_reader_.reset();
    ::rust::Box<dmg::rust_ffi::RustMachOImageReader> reader =
        dmg::rust_ffi::new_mach_o_image_reader(
            ::rust::Slice<const uint8_t>(image));
    if (!reader->is_valid()) {
      return false;
    }
    rust_reader_ = std::move(reader);
    return true;
  }

  if (image.empty()) {
    return false;
  }

  data_ = std::make_unique<ByteSlice>(image);

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
      if (!fat_images_.back()->Initialize(slice.data())) {
        return false;
      }

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
    if (!data_->CopyDataAt(offset, command->data)) {
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
    if (!data_->CopyDataAt(offset, command->data)) {
      return false;
    }

    offset += cmdsize;
  }

  return true;
}

bool MachOImageReader::IsFat() {
  if (rust_reader_.has_value()) {
    return (*rust_reader_)->is_fat();
  }
  return is_fat_;
}

std::vector<MachOImageReader*> MachOImageReader::GetFatImages() {
  DCHECK(IsFat());
  if (rust_reader_.has_value() && fat_images_.empty()) {
    // `GetFatImages()` returns non-owning `MachOImageReader*` pointers, so
    // `fat_images_` owns the thin C++ `MachOImageReader` shells wrapping each
    // Rust architecture slice for the lifetime of this reader.
    const size_t num_fat = (*rust_reader_)->num_fat_images();
    fat_images_.reserve(num_fat);
    for (size_t i = 0; i < num_fat; ++i) {
      fat_images_.push_back(base::WrapUnique(
          new MachOImageReader((*rust_reader_)->take_fat_image(i))));
    }
  }
  std::vector<MachOImageReader*> images;
  for (const auto& image : fat_images_)
    images.push_back(image.get());
  return images;
}

bool MachOImageReader::Is64Bit() {
  DCHECK(!IsFat());
  if (rust_reader_.has_value()) {
    return (*rust_reader_)->is_64_bit();
  }
  return is_64_bit_;
}

const mach_header* MachOImageReader::GetMachHeader() {
  DCHECK(!IsFat());
  if (rust_reader_.has_value()) {
    static_assert(alignof(mach_header) <= 4,
                  "AlignedMachHeader in Rust is 4-byte aligned");
    // `mach_header_bytes()` borrows from a 4-byte aligned `AlignedMachHeader`
    // owned by `rust_reader_` for the lifetime of this `MachOImageReader`.
    ::rust::Slice<const uint8_t> bytes = (*rust_reader_)->mach_header_bytes();
    if (bytes.size() < sizeof(mach_header)) {
      return nullptr;
    }
    return reinterpret_cast<const mach_header*>(bytes.data());
  }
  return data_->GetPointerAt<mach_header>(0);
}

const mach_header_64* MachOImageReader::GetMachHeader64() {
  DCHECK(Is64Bit());
  DCHECK(!IsFat());
  if (rust_reader_.has_value()) {
    static_assert(alignof(mach_header_64) <= 4,
                  "AlignedMachHeader in Rust is 4-byte aligned");
    // `mach_header_64_bytes()` borrows from a 4-byte aligned
    // `AlignedMachHeader` owned by `rust_reader_` for the lifetime of this
    // `MachOImageReader`.
    ::rust::Slice<const uint8_t> bytes =
        (*rust_reader_)->mach_header_64_bytes();
    if (bytes.size() < sizeof(mach_header_64)) {
      return nullptr;
    }
    return reinterpret_cast<const mach_header_64*>(bytes.data());
  }
  return data_->GetPointerAt<mach_header_64>(0);
}

uint32_t MachOImageReader::GetFileType() {
  DCHECK(!IsFat());
  if (rust_reader_.has_value()) {
    return (*rust_reader_)->file_type();
  }
  return GetMachHeader()->filetype;
}

std::vector<MachOImageReader::LoadCommand> MachOImageReader::GetLoadCommands() {
  DCHECK(!IsFat());
  if (rust_reader_.has_value()) {
    const size_t num_commands = (*rust_reader_)->num_load_commands();
    std::vector<LoadCommand> commands(num_commands);
    for (size_t i = 0; i < num_commands; ++i) {
      const ::rust::Slice<const uint8_t> cmd_data =
          (*rust_reader_)->load_command_data(i);
      commands[i].data.assign(cmd_data.begin(), cmd_data.end());
    }
    return commands;
  }
  return commands_;
}

bool MachOImageReader::GetCodeSignatureInfo(std::vector<uint8_t>* info) {
  DCHECK(!IsFat());
  DCHECK(info->empty());

  if (rust_reader_.has_value()) {
    if (!(*rust_reader_)->has_code_signature()) {
      return false;
    }
    ::rust::Slice<const uint8_t> sig = (*rust_reader_)->code_signature_info();
    info->assign(sig.begin(), sig.end());
    return true;
  }

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
  return data_->CopyDataAt(lc_code_signature->dataoff, *info);
}

}  // namespace safe_browsing
