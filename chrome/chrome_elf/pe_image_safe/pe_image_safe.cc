// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/pe_image_safe/pe_image_safe.h"

#include <assert.h>
#include <stddef.h>

namespace pe_image_safe {

// Sanity check.  Yes, IMAGE_NT_HEADERS is a different size depending on
// image bitness, but either way it should not be more than kPageSize.
static_assert(sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS) <= kPageSize,
              "PE Headers bigger than expected.  Very unexpected.");

//------------------------------------------------------------------------------
// Public PEImage class methods
//------------------------------------------------------------------------------

PIMAGE_DOS_HEADER PEImageSafe::GetDosHeader() {
  if (dos_header_)
    return dos_header_;

  // Find and verify the new header.
  if (!image_)
    return nullptr;

  dos_header_ = reinterpret_cast<PIMAGE_DOS_HEADER>(image_);
  if (sizeof(IMAGE_DOS_HEADER) > image_size_ ||
      dos_header_->e_magic != IMAGE_DOS_SIGNATURE) {
    dos_header_ = nullptr;
  }

  return dos_header_;
}

PIMAGE_FILE_HEADER PEImageSafe::GetFileHeader() {
  if (file_header_)
    return file_header_;

  // Find and verify the new header.
  PIMAGE_DOS_HEADER dos_header = GetDosHeader();
  if (!dos_header)
    return nullptr;

  // Note: e_lfanew is an offset from |dos_header|, which is the start of the
  //       image buffer.
  PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(
      reinterpret_cast<char*>(dos_header) + dos_header->e_lfanew);
  if (((dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS::Signature) +
        sizeof(IMAGE_FILE_HEADER)) > image_size_) ||
      nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return nullptr;
  }

  // Nothing to verify inside the Coff File Header at this point.
  file_header_ = &nt_headers->FileHeader;

  return file_header_;
}

BYTE* PEImageSafe::GetOptionalHeader() {
  if (opt_header_)
    return opt_header_;

  // Find and verify the new header.
  PIMAGE_FILE_HEADER file_header = GetFileHeader();
  if (!file_header)
    return nullptr;

  // No bitness yet...
  PIMAGE_OPTIONAL_HEADER optional_header =
      reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(
          reinterpret_cast<char*>(file_header) + sizeof(IMAGE_FILE_HEADER));
  uintptr_t optional_header_offset = reinterpret_cast<char*>(optional_header) -
                                     reinterpret_cast<char*>(dos_header_);
  if (optional_header_offset + sizeof(IMAGE_OPTIONAL_HEADER::Magic) >
      image_size_) {
    return nullptr;
  }

  // Now is the time to set the image bitness.
  if (optional_header->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    bitness_ = ImageBitness::k64;
  } else if (optional_header->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    bitness_ = ImageBitness::k32;
  } else {
    // Invalid image.
    bitness_ = ImageBitness::kUnknown;
    return nullptr;
  }

  // Sanity check that the full optional header is in this buffer.
  if ((bitness_ == ImageBitness::k64 &&
       (optional_header_offset + sizeof(IMAGE_OPTIONAL_HEADER64)) >
           image_size_) ||
      (optional_header_offset + sizeof(IMAGE_OPTIONAL_HEADER32) >
       image_size_)) {
    return nullptr;
  }

  // If |image_size_| is currently |kImageSizeNotSet| (this is an image mapped
  // into memory by NTLoader), now the size can be updated for accuracy.
  if (image_size_ == kImageSizeNotSet) {
    if (bitness_ == ImageBitness::k64) {
      image_size_ = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(optional_header)
                        ->SizeOfImage;
    } else {
      image_size_ = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(optional_header)
                        ->SizeOfImage;
    }
  }

  // Nothing to verify inside the optional header at this point.
  opt_header_ = reinterpret_cast<BYTE*>(optional_header);

  return opt_header_;
}

ImageBitness PEImageSafe::GetImageBitness() {
  // GetOptionalHeader() mines the bitness, if possible.
  if (bitness_ == ImageBitness::kUnknown)
    GetOptionalHeader();

  return bitness_;
}

//----------------------------------------------------------------------------
// The following functions are currently only supported for PE images that
// have been memory mapped by NTLoader.
//----------------------------------------------------------------------------

void* PEImageSafe::RVAToAddr(DWORD rva) {
  assert(ldr_image_mapping_);
  if (rva >= image_size_)
    return nullptr;

  return reinterpret_cast<char*>(image_) + rva;
}

void* PEImageSafe::GetImageDirectoryEntryAddr(int directory,
                                              DWORD* directory_size) {
  assert(directory >= 0 && directory < IMAGE_NUMBEROF_DIRECTORY_ENTRIES &&
         ldr_image_mapping_);

  // GetOptionalHeader() validates the optional header.
  BYTE* optional_header = GetOptionalHeader();
  if (!optional_header)
    return nullptr;

  DWORD rva = 0;
  DWORD size = 0;
  if (GetImageBitness() == ImageBitness::k64) {
    PIMAGE_OPTIONAL_HEADER64 opt_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(optional_header);
    if (directory >= static_cast<int>(opt_header->NumberOfRvaAndSizes))
      return nullptr;
    rva = opt_header->DataDirectory[directory].VirtualAddress;
    size = opt_header->DataDirectory[directory].Size;
  } else {
    PIMAGE_OPTIONAL_HEADER32 opt_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(optional_header);
    if (directory >= static_cast<int>(opt_header->NumberOfRvaAndSizes))
      return nullptr;
    rva = opt_header->DataDirectory[directory].VirtualAddress;
    size = opt_header->DataDirectory[directory].Size;
  }

  // Verify that the whole data directory is inside this PEImageSafe buffer.
  if (rva + size >= image_size_)
    return nullptr;

  if (directory_size)
    *directory_size = size;

  return RVAToAddr(rva);
}

PIMAGE_EXPORT_DIRECTORY PEImageSafe::GetExportDirectory() {
  assert(ldr_image_mapping_);

  if (export_dir_)
    return export_dir_;

  // Find and verify the new directory.
  DWORD dir_size = 0;
  export_dir_ = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(
      GetImageDirectoryEntryAddr(IMAGE_DIRECTORY_ENTRY_EXPORT, &dir_size));

  if (export_dir_) {
    // Basic sanity check.  |dir_size| will often be larger than just the given
    // base directory structure.
    if (sizeof(IMAGE_EXPORT_DIRECTORY) > dir_size)
      export_dir_ = nullptr;
  }

  return export_dir_;
}

}  // namespace pe_image_safe
