// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

#include <stdio.h>
#include <windows.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace internal {

class LnkParserTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                               &target_file_path_));
  }

  base::win::ScopedHandle CreateAndOpenShortcut(
      const base::win::ShortcutProperties& properties) {
    base::FilePath shortcut_path =
        temp_dir_.GetPath().AppendASCII("test_shortcut.lnk");
    if (!base::win::CreateOrUpdateShortcutLink(
            shortcut_path, properties,
            base::win::ShortcutOperation::kCreateAlways)) {
      LOG(ERROR) << "Could not create shortcut";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    base::File lnk_file(shortcut_path, base::File::Flags::FLAG_OPEN |
                                           base::File::Flags::FLAG_READ);
    base::win::ScopedHandle handle(lnk_file.TakePlatformFile());
    if (!handle.IsValid())
      LOG(ERROR) << "Error opening the lnk file";
    return handle;
  }

  base::win::ScopedHandle CreateAndOpenShortcutFromBuffer(
      const std::vector<BYTE>& shortcut_buffer) {
    base::FilePath shortcut_path =
        temp_dir_.GetPath().AppendASCII("test_shortcut_from_buffer.lnk");
    base::File lnk_file(shortcut_path, base::File::Flags::FLAG_CREATE |
                                           base::File::Flags::FLAG_WRITE |
                                           base::File::Flags::FLAG_READ);

    // Write the contents of the buffer to a new file.
    uint64_t written_bytes = lnk_file.Write(
        /*offset=*/0, reinterpret_cast<const char*>(shortcut_buffer.data()),
        shortcut_buffer.size());
    if (written_bytes != shortcut_buffer.size()) {
      LOG(ERROR) << "Error writing shortcut from buffer";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Rewind the file to the start.
    if (lnk_file.Seek(base::File::Whence::FROM_BEGIN, /*offset=*/0) == -1) {
      LOG(ERROR) << "Could not rewind file to the begining";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    base::win::ScopedHandle lnk_handle(lnk_file.TakePlatformFile());
    if (!lnk_handle.IsValid()) {
      LOG(ERROR) << "Cannot open lnk from buffer for writing";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }
    return lnk_handle;
  }

  void CheckParsedShortcut(ParsedLnkFile* parsed_shortcut,
                           base::FilePath target_path,
                           base::FilePath working_dir,
                           std::wstring arguments,
                           base::FilePath icon_location,
                           const int32_t& icon_index) {
    base::FilePath parsed_target_path(parsed_shortcut->target_path);
    ASSERT_TRUE(PathEqual(parsed_target_path, target_path));

    base::FilePath parsed_working_dir_file_path(parsed_shortcut->working_dir);
    ASSERT_TRUE(PathEqual(parsed_working_dir_file_path, working_dir));
    ASSERT_EQ(parsed_shortcut->command_line_arguments, arguments);

    base::FilePath parsed_icon_location(parsed_shortcut->icon_location);
    ASSERT_TRUE(PathEqual(parsed_icon_location, icon_location));
    ASSERT_EQ(parsed_shortcut->icon_index, icon_index);
  }

  bool CreateFileWithUTF16Name(base::FilePath* file_path) {
    std::wstring UTF16_file_name = L"NormalFile\x0278";
    *file_path = temp_dir_.GetPath().Append(UTF16_file_name);
    return CreateFileInFolder(temp_dir_.GetPath(), UTF16_file_name);
  }

  bool LoadShortcutIntoMemory(base::File* shortcut,
                              std::vector<BYTE>* shortcut_contents) {
    int16_t file_size = shortcut->GetLength();
    if (file_size == -1) {
      LOG(ERROR) << "Cannot get lnk file size";
      return false;
    }

    shortcut_contents->resize(file_size);

    int64_t bytes_read = shortcut->Read(
        /*offset=*/0, reinterpret_cast<char*>(shortcut_contents->data()),
        file_size);

    if (bytes_read != file_size) {
      LOG(ERROR) << "Error reading lnk file, bytes read: " << bytes_read
                 << "file size: " << file_size;
      return false;
    }

    // Assert that the file was created with more than just the
    // header.
    if (file_size <= kHeaderSize) {
      LOG(ERROR) << "Not enough data on lnk file";
      return false;
    }

    return true;
  }

  base::win::ScopedHandle CreateAndOpenShortcutToNetworkAndLocalLocation(
      const base::win::ShortcutProperties& properties) {
    base::win::ScopedHandle local_lnk_handle =
        CreateAndOpenShortcut(properties);
    if (!local_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating lnk pointing to local file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory to modify the LinkInfoFlags.
    base::File local_lnk(std::move(local_lnk_handle));
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&local_lnk, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    internal::LnkInfoPartialHeader* partial_header =
        internal::LocateAndParseLnkInfoPartialHeader(&shortcut_buffer, nullptr);

    if (!partial_header)
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);

    // The value 0x03 means that this link file points to a local file that
    // is also in a shared folder (local and network location paths).
    constexpr BYTE kLocalAndNetworkLnkFlag = 0x03;
    partial_header->flags = kLocalAndNetworkLnkFlag;

    // Write the lnk file back to disk.
    base::FilePath local_and_network_shortcut_path =
        temp_dir_.GetPath().AppendASCII("local_and_network_shortcut.lnk");
    base::File local_and_network_shortcut(local_and_network_shortcut_path,
                                          base::File::Flags::FLAG_CREATE |
                                              base::File::Flags::FLAG_WRITE |
                                              base::File::Flags::FLAG_READ);

    uint64_t written_bytes = local_and_network_shortcut.Write(
        /*offset=*/0, reinterpret_cast<const char*>(shortcut_buffer.data()),
        shortcut_buffer.size());

    if (written_bytes != shortcut_buffer.size()) {
      LOG(ERROR) << "Error writing modified shortcut";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    return base::win::ScopedHandle(
        local_and_network_shortcut.TakePlatformFile());
  }

  base::win::ScopedHandle CreateAndOpenCorruptedLnkFile(
      const base::win::ShortcutProperties& properties) {
    base::win::ScopedHandle good_lnk_handle = CreateAndOpenShortcut(properties);
    if (!good_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating and opening good lnk file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory to corrupt it.
    base::File good_lnk_file(std::move(good_lnk_handle));
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&good_lnk_file, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Leave the header intact and then corrupt every second byte
    // on the file, this was an arbitrary choice.
    uint64_t current_byte = kHeaderSize + 0x02;

    while (current_byte < shortcut_buffer.size()) {
      shortcut_buffer[current_byte] = 0xff;
      current_byte += 2;
    }

    return CreateAndOpenShortcutFromBuffer(shortcut_buffer);
  }

  // Creates a shortcut file and pads |extra_size| extra bytes to the end of it.
  base::win::ScopedHandle CreateAndOpenPaddedLnkFile(
      const base::win::ShortcutProperties& properties,
      size_t extra_size) {
    base::win::ScopedHandle good_lnk_handle = CreateAndOpenShortcut(properties);
    if (!good_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating and opening good lnk file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory.
    base::File good_lnk_file(std::move(good_lnk_handle));
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&good_lnk_file, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Add extra bytes at the end of the buffer.
    size_t initial_size = shortcut_buffer.size();
    shortcut_buffer.resize(initial_size + extra_size);
    std::fill(shortcut_buffer.begin() + initial_size, shortcut_buffer.end(), 1);

    return CreateAndOpenShortcutFromBuffer(shortcut_buffer);
  }

  // Returns the last found instance of a given "StringData" in the given
  // buffer. A StringData consists of a 16-bit unsigned integer which represents
  // the size for a string followed by the variable-length unicode string. See
  // https://msdn.microsoft.com/en-us/library/dd871305.aspx for more
  // information.
  bool FindStringDataInBuffer(const std::vector<BYTE>& buffer,
                              const std::wstring& expected_string,
                              DWORD* found_location) {
    size_t length = expected_string.length();
    if (buffer.size() < length + 2)
      return false;

    bool found = false;
    char size_upper = length >> 8;
    char size_lower = length % (1 << 8);
    for (size_t i = 0; i < buffer.size() - length - 1; i++) {
      if (buffer[i] == size_lower && buffer[i + 1] == size_upper) {
        const wchar_t* string_ptr =
            reinterpret_cast<const wchar_t*>(buffer.data() + i + 2);
        std::wstring found_string(string_ptr, length);
        if (found_string == expected_string) {
          found = true;
          *found_location = i;
        }
      }
    }

    return found;
  }

  // Creates a shortcut file and modifies its bytes containing the size of
  // the command-line arguments.
  base::win::ScopedHandle CreateAndOpenCorruptedArgumentSizeLnkFile(
      const base::win::ShortcutProperties& properties,
      SHORT new_size) {
    base::win::ScopedHandle good_lnk_handle = CreateAndOpenShortcut(properties);
    if (!good_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating and opening good lnk file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory.
    base::File good_lnk_file(std::move(good_lnk_handle));
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&good_lnk_file, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Find the location of the command-line argument size bytes.
    DWORD argument_size_location;
    if (!FindStringDataInBuffer(shortcut_buffer, properties.arguments,
                                &argument_size_location)) {
      LOG(ERROR) << "Error finding argument locations";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }
    // Set the command-line argument bytes to |new_size|.
    shortcut_buffer[argument_size_location] = new_size % (1 << 8);
    shortcut_buffer[argument_size_location + 1] = new_size >> 8;

    return CreateAndOpenShortcutFromBuffer(shortcut_buffer);
  }

 protected:
  base::FilePath target_file_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(LnkParserTest, ParseLnkWithoutArgumentsTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;

  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, base::FilePath(L""),
                      L"", base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, ParseLnkWithArgumentsTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(target_file_path_);
  const std::wstring kArguments = L"Seven";
  properties.set_arguments(kArguments.c_str());

  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, target_file_path_,
                      kArguments, base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, ParseLnkWithExtraStringStructures) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  const std::wstring kArguments =
      L"lavidaesbella --arg1 --argmento2 --argumento3 /a --Seven";
  properties.set_arguments(kArguments.c_str());

  properties.set_working_dir(temp_dir_.GetPath());
  properties.set_app_id(L"id");

  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, temp_dir_.GetPath(),
                      kArguments, base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, UTF16FileNameParseTest) {
  base::FilePath utf16_path;
  ASSERT_TRUE(CreateFileWithUTF16Name(&utf16_path));
  base::win::ShortcutProperties properties;
  properties.set_target(utf16_path);
  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, utf16_path, base::FilePath(L""), L"",
                      base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, InvalidHandleTest) {
  base::win::ScopedHandle invalid_handle(INVALID_HANDLE_VALUE);
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(invalid_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, NoShortcutFileTest) {
  base::File no_shortcut(target_file_path_, base::File::Flags::FLAG_OPEN |
                                                base::File::Flags::FLAG_READ);
  base::win::ScopedHandle handle(no_shortcut.TakePlatformFile());

  ASSERT_TRUE(handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, CorruptedShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  base::win::ScopedHandle bad_lnk_handle =
      CreateAndOpenCorruptedLnkFile(properties);

  ASSERT_TRUE(bad_lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(bad_lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, ReasonablyLargeFileSizeShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  // Create a LNK file with an extra kilobyte of data appended to it.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenPaddedLnkFile(properties, 1024);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, temp_dir_.GetPath(),
                      L"", base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, TooLargeFileSizeShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  // Create a LNK file with an extra megabyte of data appended to it.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenPaddedLnkFile(properties, 1024 * 1024);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, ArgumentsSizeCorruptedShortcutTest_TooLarge) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());
  const std::wstring kArguments = L"foo --bar";
  properties.set_arguments(kArguments.c_str());

  // Create a LNK file which thinks its arguments are much longer than they
  // really are.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenCorruptedArgumentSizeLnkFile(properties, 256);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, ArgumentsSizeCorruptedShortcutTest_ZeroSize) {
  const std::wstring kArguments = L"foo --bar";

  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());
  properties.set_arguments(kArguments.c_str());

  // Create a LNK file which thinks its arguments are of size zero.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenCorruptedArgumentSizeLnkFile(properties, 0);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, ArgumentsSizeCorruptedShortcutTest_NegativeSize) {
  const std::wstring kArguments = L"foo --bar";

  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());
  properties.set_arguments(kArguments.c_str());

  // Create a LNK file which thinks its arguments are of a negative size.
  // The size is supposed to be unsigned, so a negative size should have the
  // same behavior as if the size was much larger than the real file.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenCorruptedArgumentSizeLnkFile(properties, -42);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, ArgumentsSizeCorruptedShortcutTest_Smaller) {
  const std::wstring kArguments = L"foo --bar";

  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());
  properties.set_arguments(kArguments.c_str());

  // Create a LNK file which thinks its arguments are smaller than they
  // really are.
  base::win::ScopedHandle lnk_handle =
      CreateAndOpenCorruptedArgumentSizeLnkFile(properties, 3);

  ASSERT_TRUE(lnk_handle.IsValid());
  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, temp_dir_.GetPath(),
                      L"foo", base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, LocalAndNetworkShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  base::win::ScopedHandle local_and_network_lnk_handle =
      CreateAndOpenShortcutToNetworkAndLocalLocation(properties);

  ASSERT_TRUE(local_and_network_lnk_handle.IsValid());
  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(local_and_network_lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, temp_dir_.GetPath(),
                      L"", base::FilePath(L""), properties.icon_index);
}

TEST_F(LnkParserTest, ParseIconLocationTest) {
  base::FilePath txt_file_path = temp_dir_.GetPath().Append(L"file.txt");
  base::File txt_file(txt_file_path, base::File::Flags::FLAG_CREATE |
                                         base::File::Flags::FLAG_READ);
  ASSERT_TRUE(txt_file.IsValid());

  int32_t icon_index = 0;
  base::win::ShortcutProperties properties;
  properties.set_target(txt_file_path);
  properties.set_icon(txt_file_path, icon_index);

  base::win::ScopedHandle txt_lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(txt_lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  EXPECT_EQ(ParseLnk(std::move(txt_lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, txt_file_path, base::FilePath(L""), L"",
                      txt_file_path, icon_index);
}

}  // namespace internal

}  // namespace chrome_cleaner
