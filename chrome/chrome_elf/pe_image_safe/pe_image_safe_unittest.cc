// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/pe_image_safe/pe_image_safe.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pe_image_safe {
namespace {

constexpr wchar_t kPeFile[] = L"chrome_elf.dll";
constexpr DWORD kPageSize = 4096;

struct CompareData {
  bool bits_64;
  DWORD size_of_image;
  DWORD time_date_stamp;
  PIMAGE_DOS_HEADER dos_header;
  PIMAGE_NT_HEADERS nt_headers;
  PIMAGE_FILE_HEADER file_header;
  PIMAGE_OPTIONAL_HEADER optional_header;
};

// Raw collection of some PE header data, without using pe_image_safe.
// This function assumes full headers from a legitimate PE image.
bool GetComparisonData(char* buffer, CompareData* data) {
  data->dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);
  if (data->dos_header->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  data->nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(
      reinterpret_cast<char*>(data->dos_header) + data->dos_header->e_lfanew);
  if (data->nt_headers->Signature != IMAGE_NT_SIGNATURE)
    return false;

  data->file_header = &data->nt_headers->FileHeader;
  data->time_date_stamp = data->nt_headers->FileHeader.TimeDateStamp;
  data->optional_header = &data->nt_headers->OptionalHeader;

  if (data->optional_header->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    PIMAGE_OPTIONAL_HEADER64 optional_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(
            &data->nt_headers->OptionalHeader);
    data->bits_64 = true;
    data->size_of_image = optional_header->SizeOfImage;
  } else {
    PIMAGE_OPTIONAL_HEADER32 optional_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(
            &data->nt_headers->OptionalHeader);
    data->bits_64 = false;
    data->size_of_image = optional_header->SizeOfImage;
  }

  return true;
}

//------------------------------------------------------------------------------

TEST(PEImageSafe, SanityTest) {
  // Open and read in a PE file.
  base::FilePath pe_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &pe_path));
  pe_path = pe_path.Append(kPeFile);

  base::File file(pe_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  std::vector<char> buffer;
  buffer.resize(kPageSize);
  ASSERT_EQ(file.Read(0, &buffer[0], kPageSize), static_cast<int>(kPageSize));
  file.Close();

  // Grab some key data out of the pe headers first, NOT using pe_image_safe.
  CompareData data_for_comparison = {};
  ASSERT_TRUE(GetComparisonData(buffer.data(), &data_for_comparison));

  // Apply scaffolding.
  PEImageSafe pe_image(buffer.data(), static_cast<DWORD>(buffer.size()));
  PIMAGE_DOS_HEADER dos_header = pe_image.GetDosHeader();
  EXPECT_TRUE(dos_header);
  EXPECT_EQ(dos_header, data_for_comparison.dos_header);

  PIMAGE_FILE_HEADER file_header = pe_image.GetFileHeader();
  EXPECT_TRUE(file_header);
  EXPECT_EQ(file_header, data_for_comparison.file_header);
  EXPECT_EQ(file_header->TimeDateStamp, data_for_comparison.time_date_stamp);

  BYTE* optional_header = pe_image.GetOptionalHeader();
  EXPECT_TRUE(optional_header);
  EXPECT_EQ(optional_header,
            reinterpret_cast<BYTE*>(data_for_comparison.optional_header));

  pe_image_safe::ImageBitness bitness = pe_image.GetImageBitness();
  EXPECT_NE(bitness, pe_image_safe::ImageBitness::kUnknown);
  EXPECT_TRUE(((bitness == pe_image_safe::ImageBitness::k64) &&
               data_for_comparison.bits_64) ||
              ((bitness == pe_image_safe::ImageBitness::k32) &&
               !data_for_comparison.bits_64));

  if (bitness == pe_image_safe::ImageBitness::k64) {
    PIMAGE_OPTIONAL_HEADER64 optional_header64 =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(optional_header);
    EXPECT_EQ(optional_header64->SizeOfImage,
              data_for_comparison.size_of_image);
  } else {
    PIMAGE_OPTIONAL_HEADER32 optional_header32 =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(optional_header);
    EXPECT_EQ(optional_header32->SizeOfImage,
              data_for_comparison.size_of_image);
  }
}

}  // namespace
}  // namespace pe_image_safe
