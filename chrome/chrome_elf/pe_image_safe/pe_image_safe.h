// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(pennymac): Consider merging into base/win/PEImage. crbug/772168

#ifndef CHROME_CHROME_ELF_PE_IMAGE_SAFE_PE_IMAGE_SAFE_H_
#define CHROME_CHROME_ELF_PE_IMAGE_SAFE_PE_IMAGE_SAFE_H_

#include <windows.h>

#include <limits>

// https://msdn.microsoft.com/library/windows/desktop/ms684179.aspx
#define LDR_IS_DATAFILE(handle) (((ULONG_PTR)(handle)) & (ULONG_PTR)1)

namespace pe_image_safe {

constexpr DWORD kPageSize = 4096;
constexpr DWORD kImageSizeNotSet = std::numeric_limits<DWORD>::max();

enum class ImageBitness { kUnknown, k32, k64 };

// This class is useful as scaffolding around an existing memory buffer, that
// should be a whole or partial Portable Executable (PE) image.
// - This class does NOT trust the contents of the given memory, and handles
//   parsing in a safe manor.
// - The caller retains ownership of the given memory throughout use of this
//   class.  Do not free the memory while using an instance.
class PEImageSafe {
 public:
  PEImageSafe(PEImageSafe&&) noexcept = default;
  PEImageSafe& operator=(PEImageSafe&&) noexcept = default;

  // The constructor does not do any parsing of the image buffer.
  // Parsing only happens as needed, on demand, via the class methods.
  // - As key points in the PE are discovered, they are saved internally for
  //   optimization.
  // - |buffer_size| can represent a partial PE image, but |buffer| must
  //   reference the start of the PE to successfully parse anything.
  PEImageSafe(BYTE* buffer, DWORD buffer_size)
      : PEImageSafe(reinterpret_cast<void*>(buffer), buffer_size) {}

  PEImageSafe(void* buffer, DWORD buffer_size) : image_size_(buffer_size) {
    image_ = reinterpret_cast<HMODULE>(buffer);
  }

  // Some functions can only be used on images that have been memory mapped by
  // the NT Loader (e.g. LoadLibrary).  This constructor must be used to enable
  // that functionality.
  // - Note: If the mapped image size is not known (e.g. via LoadLibrary), pass
  //   kImageSizeNotSet for |buffer_size|.  The value will be mined from the PE
  //   headers.
  // - Note: Full load or LOAD_LIBRARY_AS_IMAGE_RESOURCE required.
  // LOAD_LIBRARY_AS_DATAFILE* is not sufficient for some APIs.
  PEImageSafe(HMODULE buffer, DWORD buffer_size)
      : image_(buffer), image_size_(buffer_size) {
    ldr_image_mapping_ = !LDR_IS_DATAFILE(buffer);
  }

  // Return a pointer to the PE Dos Header.
  // - Returns null if the image buffer is too small or if the image is
  //   invalid.
  PIMAGE_DOS_HEADER GetDosHeader();

  // Return a pointer to the PE COFF File Header.
  // - Returns null if the image buffer is too small or if the image is
  //   invalid.
  PIMAGE_FILE_HEADER GetFileHeader();
  // Return a pointer to the PE Optional Header.
  // - Returns null if the image buffer is too small or if the image is
  //   invalid.
  // - Based on GetImageBitness(), reinterpret the returned pointer to either
  //   PIMAGE_OPTIONAL_HEADER64 or PIMAGE_OPTIONAL_HEADER32.
  BYTE* GetOptionalHeader();

  // Returns the bitness of this PE image.
  // - Returns ImageBitness::Unknown if the image buffer is too small to
  //   determine, or if the image is invalid.
  // - NOTE: PE header definitions are the same between 32 and 64-bit, right up
  //   until the |Magic| field in the Optional Header.
  ImageBitness GetImageBitness();

  //----------------------------------------------------------------------------
  // The following functions are currently only supported for PE images that
  // have been memory mapped by NTLoader.
  //----------------------------------------------------------------------------

  // Converts a Relative Virtual Address (RVA) to direct pointer.
  // - If |rva| >= |image_size_|, returns nullptr.
  void* RVAToAddr(DWORD rva);

  // Returns the address of a given directory entry.
  // - |directory| should be a Windows define from winnt.h
  //   E.g.: IMAGE_DIRECTORY_ENTRY_EXPORT
  // - Reinterpret the returned pointer based on the directory requested.
  // - Returns null if the image buffer is too small or if the image is
  //   invalid.
  // - On success, |directory_size| will hold the value from
  //   IMAGE_DATA_DIRECTORY::Size.  Pass null if size not wanted.
  void* GetImageDirectoryEntryAddr(int directory, DWORD* directory_size);

  // Small wrapper of GetImageDirectoryEntryAddr() to get a pointer to the
  // Export Directory.
  // - This function does not verify any fields inside the
  //   IMAGE_EXPORT_DIRECTORY, just that it is the expected size.
  PIMAGE_EXPORT_DIRECTORY GetExportDirectory();

  PEImageSafe(const PEImageSafe&) = delete;
  PEImageSafe& operator=(const PEImageSafe&) = delete;

 private:
  HMODULE image_ = nullptr;
  DWORD image_size_ = 0;
  ImageBitness bitness_ = ImageBitness::kUnknown;
  PIMAGE_DOS_HEADER dos_header_ = nullptr;
  PIMAGE_FILE_HEADER file_header_ = nullptr;
  BYTE* opt_header_ = nullptr;
  bool ldr_image_mapping_ = false;
  PIMAGE_EXPORT_DIRECTORY export_dir_ = nullptr;
};

}  // namespace pe_image_safe

#endif  // CHROME_CHROME_ELF_PE_IMAGE_SAFE_PE_IMAGE_SAFE_H_
