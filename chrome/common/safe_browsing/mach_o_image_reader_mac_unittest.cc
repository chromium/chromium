// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"

#include <arpa/inet.h>
#include <libkern/OSByteOrder.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/vm_param.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <uuid/uuid.h>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

// Definitions from
// <http://opensource.apple.com/source/xnu/xnu-2782.1.97/bsd/sys/codesign.h>.

enum {
  CSMAGIC_CODEDIRECTORY = 0xfade0c02,
  CSMAGIC_EMBEDDED_SIGNATURE = 0xfade0cc0,

  CSSLOT_CODEDIRECTORY = 0,
};

struct CodeSigningBlob {
  uint32_t type;
  uint32_t offset;
};

struct CodeSigningSuperBlob {
  uint32_t magic;
  uint32_t length;
  uint32_t count;
  CodeSigningBlob index[];
};

struct CodeSigningDirectory {
  uint32_t magic;
  uint32_t length;
  uint32_t version;
  uint32_t flags;
  uint32_t hashOffset;
  uint32_t identOffset;
  uint32_t nSpecialSlots;
  uint32_t nCodeSlots;
  uint32_t codeLimit;
  uint8_t hashSize;
  uint8_t hashType;
  uint8_t spare1;
  uint8_t pageSize;
  uint32_t spare2;
  // Version 0x20100.
  uint32_t scatterOffset;
  // Version 0x20200.
  uint32_t teamOffset;
};

class MachOImageReaderTest : public testing::Test {
 protected:
  void OpenTestFile(const char* file_name, base::MemoryMappedFile* file) {
    base::FilePath test_data;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));

    base::FilePath path = test_data.AppendASCII("safe_browsing")
                                   .AppendASCII("mach_o")
                                   .AppendASCII(file_name);

    ASSERT_TRUE(file->Initialize(path));
  }

  // Returns the identity of the signed code data.
  void GetSigningIdentity(const std::vector<uint8_t>& signature,
                          std::string* identity) {
    auto* super_blob =
        reinterpret_cast<const CodeSigningSuperBlob*>(&signature[0]);
    EXPECT_EQ(CSMAGIC_EMBEDDED_SIGNATURE, ntohl(super_blob->magic));
    ASSERT_EQ(CSSLOT_CODEDIRECTORY, ntohl(super_blob->index[0].type));
    size_t dir_offset = ntohl(super_blob->index[0].offset);
    auto* directory =
        reinterpret_cast<const CodeSigningDirectory*>(&signature[dir_offset]);
    ASSERT_EQ(CSMAGIC_CODEDIRECTORY, ntohl(directory->magic));
    size_t ident_offset = ntohl(directory->identOffset) + dir_offset;
    *identity =
        std::string(reinterpret_cast<const char*>(&signature[ident_offset]));
  }

  // Returns the hash of the code data itself. Note that this is not the
  // CDHash, but is instead the hash in the CodeDirectory blob, which is
  // over the contents of the signed data. This is visible as hash #0
  // when using `codesign -d -vvvvvv`.
  void GetCodeSignatureHash(const std::vector<uint8_t>& signature,
                            std::vector<uint8_t>* hash) {
    auto* super_blob =
        reinterpret_cast<const CodeSigningSuperBlob*>(&signature[0]);
    EXPECT_EQ(CSMAGIC_EMBEDDED_SIGNATURE, ntohl(super_blob->magic));
    ASSERT_EQ(CSSLOT_CODEDIRECTORY, ntohl(super_blob->index[0].type));
    size_t dir_offset = ntohl(super_blob->index[0].offset);
    auto* directory =
        reinterpret_cast<const CodeSigningDirectory*>(&signature[dir_offset]);
    ASSERT_EQ(CSMAGIC_CODEDIRECTORY, ntohl(directory->magic));
    size_t hash_offset = ntohl(directory->hashOffset) + dir_offset;
    std::vector<uint8_t> actual_hash(&signature[hash_offset],
        &signature[hash_offset + directory->hashSize]);
    EXPECT_EQ(20u, actual_hash.size());
    *hash = actual_hash;
  }

  void ExpectCodeSignatureHash(const std::vector<uint8_t>& signature,
                               const char* expected) {
    std::vector<uint8_t> actual_hash;
    GetCodeSignatureHash(signature, &actual_hash);

    std::vector<uint8_t> expected_hash;
    ASSERT_TRUE(base::HexStringToBytes(expected, &expected_hash));
    EXPECT_EQ(expected_hash, actual_hash);
  }
};

TEST_F(MachOImageReaderTest, Executable32) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("executable32", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_FALSE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), reader.GetFileType());
  EXPECT_EQ(15u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_FALSE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_TRUE(signature.empty());

  // Test an arbitrary load command.
  auto commands = reader.GetLoadCommands();
  ASSERT_EQ(15u, commands.size());
  auto command = commands[11];
  ASSERT_EQ(static_cast<uint32_t>(LC_LOAD_DYLIB), command.cmd());
  auto* actual = command.as_command<dylib_command>();
  EXPECT_EQ(2u, actual->dylib.timestamp);
  EXPECT_EQ(0x4ad0101u, actual->dylib.current_version);
  EXPECT_EQ(0x10000u, actual->dylib.compatibility_version);
}

TEST_F(MachOImageReaderTest, Executable64) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("executable64", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_TRUE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_TRUE(reader.GetMachHeader64());
  EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), reader.GetFileType());
  EXPECT_EQ(15u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_FALSE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_TRUE(signature.empty());
}

TEST_F(MachOImageReaderTest, ExecutableFat) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("executablefat", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_TRUE(reader.IsFat());
  auto images = reader.GetFatImages();
  ASSERT_EQ(2u, images.size());

  // Note: this image is crafted to have 32-bit first.
  {
    EXPECT_FALSE(images[0]->IsFat());
    EXPECT_FALSE(images[0]->Is64Bit());
    EXPECT_TRUE(images[0]->GetMachHeader());
    EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), images[0]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_FALSE(images[0]->GetCodeSignatureInfo(&signature));
    EXPECT_TRUE(signature.empty());
  }

  {
    EXPECT_FALSE(images[1]->IsFat());
    EXPECT_TRUE(images[1]->Is64Bit());
    EXPECT_TRUE(images[1]->GetMachHeader());
    EXPECT_TRUE(images[1]->GetMachHeader64());
    EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), images[1]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_FALSE(images[1]->GetCodeSignatureInfo(&signature));
    EXPECT_TRUE(signature.empty());

    // Test an arbitrary load command.
    auto commands = images[1]->GetLoadCommands();
    ASSERT_EQ(15u, commands.size());
    auto command = commands[1];
    ASSERT_EQ(static_cast<uint32_t>(LC_SEGMENT_64), command.cmd());
    auto* actual = command.as_command<segment_command_64>();
    EXPECT_EQ("__TEXT", std::string(actual->segname));
    EXPECT_EQ(0u, actual->fileoff);
    EXPECT_EQ(4096u, actual->filesize);
    EXPECT_EQ(0x7, actual->maxprot);
    EXPECT_EQ(0x5, actual->initprot);
    EXPECT_EQ(3u, actual->nsects);
    EXPECT_EQ(0u, actual->flags);
  }
}

TEST_F(MachOImageReaderTest, ExecutablePPC) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("executableppc", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_FALSE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_EQ(OSSwapInt32(MH_EXECUTE), reader.GetFileType());
  EXPECT_EQ(10u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_FALSE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_TRUE(signature.empty());
}

TEST_F(MachOImageReaderTest, Dylib32) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("lib32.dylib", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_FALSE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_EQ(static_cast<uint32_t>(MH_DYLIB), reader.GetFileType());
  EXPECT_EQ(13u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_FALSE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_TRUE(signature.empty());
}

TEST_F(MachOImageReaderTest, Dylib64) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("lib64.dylib", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_TRUE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_TRUE(reader.GetMachHeader64());
  EXPECT_EQ(static_cast<uint32_t>(MH_DYLIB), reader.GetFileType());
  EXPECT_EQ(13u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_FALSE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_TRUE(signature.empty());

  // Test an arbitrary load command.
  auto commands = reader.GetLoadCommands();
  ASSERT_EQ(13u, commands.size());
  auto command = commands[6];
  ASSERT_EQ(static_cast<uint32_t>(LC_UUID), command.cmd());
  uuid_t expected = {0xB6, 0xB5, 0x12, 0xD7,
                     0x64, 0xE9,
                     0x3F, 0x7A,
                     0xAB, 0x4A,
                     0x87, 0x46, 0x36, 0x76, 0x87, 0x47};
  EXPECT_EQ(0, uuid_compare(expected,
                            command.as_command<uuid_command>()->uuid));
}

TEST_F(MachOImageReaderTest, DylibFat) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("libfat.dylib", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_TRUE(reader.IsFat());
  auto images = reader.GetFatImages();
  ASSERT_EQ(2u, images.size());

  // Note: this image is crafted to have 64-bit first.
  {
    EXPECT_FALSE(images[0]->IsFat());
    EXPECT_TRUE(images[0]->Is64Bit());
    EXPECT_TRUE(images[0]->GetMachHeader());
    EXPECT_TRUE(images[0]->GetMachHeader64());
    EXPECT_EQ(static_cast<uint32_t>(MH_DYLIB), images[0]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_FALSE(images[0]->GetCodeSignatureInfo(&signature));
    EXPECT_TRUE(signature.empty());
  }

  {
    EXPECT_FALSE(images[1]->IsFat());
    EXPECT_FALSE(images[1]->Is64Bit());
    EXPECT_TRUE(images[1]->GetMachHeader());
    EXPECT_EQ(static_cast<uint32_t>(MH_DYLIB), images[1]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_FALSE(images[1]->GetCodeSignatureInfo(&signature));
    EXPECT_TRUE(signature.empty());
  }
}

TEST_F(MachOImageReaderTest, SignedExecutable32) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("signedexecutable32", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_FALSE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), reader.GetFileType());
  EXPECT_EQ(16u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_TRUE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_EQ(9344u, signature.size());

  std::string identity;
  GetSigningIdentity(signature, &identity);
  EXPECT_EQ("signedexecutable32", identity);

  ExpectCodeSignatureHash(signature,
                          "11fb88eb63c10dfc3d24a2545ea2a9c50c2921b5");
}

TEST_F(MachOImageReaderTest, SignedExecutableFat) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("signedexecutablefat", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_TRUE(reader.IsFat());
  auto images = reader.GetFatImages();
  ASSERT_EQ(2u, images.size());

  // Note: this image is crafted to have 32-bit first.
  {
    EXPECT_FALSE(images[0]->IsFat());
    EXPECT_FALSE(images[0]->Is64Bit());
    EXPECT_TRUE(images[0]->GetMachHeader());
    EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), images[0]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_TRUE(images[0]->GetCodeSignatureInfo(&signature));
    EXPECT_EQ(9344u, signature.size());

    std::string identity;
    GetSigningIdentity(signature, &identity);
    EXPECT_EQ("signedexecutablefat", identity);

    ExpectCodeSignatureHash(signature,
                            "11fb88eb63c10dfc3d24a2545ea2a9c50c2921b5");
  }

  {
    EXPECT_FALSE(images[1]->IsFat());
    EXPECT_TRUE(images[1]->Is64Bit());
    EXPECT_TRUE(images[1]->GetMachHeader());
    EXPECT_TRUE(images[1]->GetMachHeader64());
    EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), images[1]->GetFileType());

    std::vector<uint8_t> signature;
    EXPECT_TRUE(images[1]->GetCodeSignatureInfo(&signature));
    EXPECT_EQ(9344u, signature.size());

    std::string identity;
    GetSigningIdentity(signature, &identity);
    EXPECT_EQ("signedexecutablefat", identity);

    ExpectCodeSignatureHash(signature,
                            "750a57326ba85857371094900475defd837f5e14");
  }
}

TEST_F(MachOImageReaderTest, SignedDylib64) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("libsigned64.dylib", &file));
  MachOImageReader reader;
  ASSERT_TRUE(reader.Initialize(file.data(), file.length()));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_TRUE(reader.Is64Bit());
  EXPECT_TRUE(reader.GetMachHeader());
  EXPECT_TRUE(reader.GetMachHeader64());
  EXPECT_EQ(static_cast<uint32_t>(MH_DYLIB), reader.GetFileType());
  EXPECT_EQ(14u, reader.GetLoadCommands().size());

  std::vector<uint8_t> signature;
  EXPECT_TRUE(reader.GetCodeSignatureInfo(&signature));
  EXPECT_EQ(9328u, signature.size());

  std::string identity;
  GetSigningIdentity(signature, &identity);
  EXPECT_EQ("libsigned64", identity);

  ExpectCodeSignatureHash(signature,
                          "8b1c79b60bb53a7f17b5618d5feb10dc8b88d806");
}

TEST_F(MachOImageReaderTest, NotMachO) {
  base::MemoryMappedFile file;
  ASSERT_NO_FATAL_FAILURE(OpenTestFile("src.c", &file));
  MachOImageReader reader;
  EXPECT_FALSE(reader.Initialize(file.data(), file.length()));
}

TEST_F(MachOImageReaderTest, IsMachOMagicValue) {
  static const uint32_t kMagics[] = { MH_MAGIC, MH_MAGIC, FAT_MAGIC };
  for (uint32_t magic : kMagics) {
    SCOPED_TRACE(base::StringPrintf("0x%x", magic));
    EXPECT_TRUE(MachOImageReader::IsMachOMagicValue(magic));
    EXPECT_TRUE(MachOImageReader::IsMachOMagicValue(OSSwapInt32(magic)));
  }
}

// https://crbug.com/524044
TEST_F(MachOImageReaderTest, CmdsizeSmallerThanLoadCommand) {
#pragma pack(push, 1)
  struct TestImage {
    mach_header_64 header;
    segment_command_64 page_zero;
    load_command small_sized;
    segment_command_64 fake_code;
  };
#pragma pack(pop)

  TestImage test_image = {};

  test_image.header.magic = MH_MAGIC_64;
  test_image.header.cputype = CPU_TYPE_X86_64;
  test_image.header.filetype = MH_EXECUTE;
  test_image.header.ncmds = 3;
  test_image.header.sizeofcmds = sizeof(test_image) - sizeof(test_image.header);

  test_image.page_zero.cmd = LC_SEGMENT;
  test_image.page_zero.cmdsize = sizeof(test_image.page_zero);
  strcpy(test_image.page_zero.segname, SEG_PAGEZERO);
  test_image.page_zero.vmsize = PAGE_SIZE;

  test_image.small_sized.cmd = LC_SYMSEG;
  test_image.small_sized.cmdsize = sizeof(test_image.small_sized) - 3;

  test_image.fake_code.cmd = LC_SEGMENT;
  test_image.fake_code.cmdsize = sizeof(test_image.fake_code);
  strcpy(test_image.fake_code.segname, SEG_TEXT);

  MachOImageReader reader;
  EXPECT_TRUE(reader.Initialize(reinterpret_cast<const uint8_t*>(&test_image),
                                sizeof(test_image)));

  EXPECT_FALSE(reader.IsFat());
  EXPECT_TRUE(reader.Is64Bit());

  const auto& load_commands = reader.GetLoadCommands();
  EXPECT_EQ(3u, load_commands.size());

  EXPECT_EQ(static_cast<uint32_t>(LC_SEGMENT), load_commands[0].cmd());
  EXPECT_EQ(static_cast<uint32_t>(LC_SYMSEG), load_commands[1].cmd());
  EXPECT_EQ(sizeof(load_command) - 3, load_commands[1].cmdsize());
  EXPECT_EQ(static_cast<uint32_t>(LC_SEGMENT), load_commands[2].cmd());
}

// https://crbug.com/591194
TEST_F(MachOImageReaderTest, RecurseFatHeader) {
#pragma pack(push, 1)
  struct TestImage {
    fat_header header;
    fat_arch arch1;
    fat_arch arch2;
    mach_header_64 macho64;
    mach_header macho;
  };
#pragma pack(pop)

  TestImage test_image = {};
  test_image.header.magic = FAT_MAGIC;
  test_image.header.nfat_arch = 2;
  test_image.arch1.offset = offsetof(TestImage, macho64);
  test_image.arch1.size = sizeof(mach_header_64);
  test_image.arch2.offset = 0;  // Cannot point back at the fat_header.
  test_image.arch2.size = sizeof(test_image);

  test_image.macho64.magic = MH_MAGIC_64;
  test_image.macho.magic = MH_MAGIC;

  MachOImageReader reader;
  EXPECT_FALSE(reader.Initialize(reinterpret_cast<const uint8_t*>(&test_image),
                                 sizeof(test_image)));
}

}  // namespace
}  // namespace safe_browsing
