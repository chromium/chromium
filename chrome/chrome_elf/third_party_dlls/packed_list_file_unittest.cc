// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/pe_image.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/chrome_elf/sha1/sha1.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/user_data_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace third_party_dlls {
namespace {

constexpr wchar_t kTestBlFileName[] = L"blfile";
constexpr DWORD kPageSize = 4096;

void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager* rom) {
  ASSERT_NE(key, nt::AUTO);
  HKEY root = (key == nt::HKCU ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE);
  base::string16 temp;

  ASSERT_NO_FATAL_FAILURE(rom->OverrideRegistry(root, &temp));
  ASSERT_TRUE(nt::SetTestingOverride(key, temp));
}

void CancelRegRedirect(nt::ROOT_KEY key) {
  ASSERT_NE(key, nt::AUTO);
  ASSERT_TRUE(nt::SetTestingOverride(key, base::string16()));
}

bool CreateRegistryKeyValue(const base::string16& full_file_path) {
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER,
                 install_static::GetRegistryPath()
                     .append(kThirdPartyRegKeyName)
                     .c_str(),
                 KEY_WRITE) != ERROR_SUCCESS ||
      !key.Valid()) {
    return false;
  }
  if (key.WriteValue(kBlFilePathRegValue, full_file_path.c_str()) !=
      ERROR_SUCCESS)
    return false;

  return true;
}

struct TestModule {
  std::string basename;
  DWORD timedatestamp;
  DWORD imagesize;
};

bool GetTestModules(std::vector<TestModule>* test_modules,
                    std::vector<PackedListModule>* packed_modules) {
  // Test binaries in system32/syswow64.
  // Define them in hash order so that the resulting array is ordered!
  // ole32 = 65 6e 16..., gdi32 = 91 7a e5..., crypt32 = ce ab 70...
  static constexpr const wchar_t* kTestBins[] = {L"ole32.dll", L"gdi32.dll",
                                                 L"gdi32.dll", L"crypt32.dll"};

  // Get test data from system binaries.
  base::FilePath path;
  char buffer[kPageSize];
  PIMAGE_NT_HEADERS nt_headers = nullptr;
  test_modules->clear();
  packed_modules->clear();

  for (const wchar_t* test_bin : kTestBins) {
    if (!base::PathService::Get(base::DIR_SYSTEM, &path))
      return false;
    path = path.Append(test_bin);
    base::File binary(path, base::File::FLAG_READ | base::File::FLAG_OPEN);
    if (!binary.IsValid())
      return false;
    if (binary.Read(0, &buffer[0], kPageSize) != kPageSize)
      return false;
    base::win::PEImage pe_image(buffer);
    if (!pe_image.VerifyMagic())
      return false;
    nt_headers = pe_image.GetNTHeaders();

    // Save the module info for tests.
    TestModule test_module;
    test_module.basename = base::UTF16ToASCII(test_bin);
    test_module.timedatestamp = nt_headers->FileHeader.TimeDateStamp;
    test_module.imagesize = nt_headers->OptionalHeader.SizeOfImage;
    test_modules->push_back(test_module);

    // SHA1 hash the two strings, and copy them into the module array.
    PackedListModule packed_module;
    packed_module.code_id_hash = elf_sha1::SHA1HashString(
        GetFingerprintString(test_module.timedatestamp, test_module.imagesize));
    packed_module.basename_hash =
        elf_sha1::SHA1HashString(test_module.basename);
    packed_modules->push_back(packed_module);
  }

  return true;
}

//------------------------------------------------------------------------------
// ThirdPartyFileTest class
//------------------------------------------------------------------------------

class ThirdPartyFileTest : public testing::Test {
 protected:
  ThirdPartyFileTest() = default;

  void SetUp() override {
    ASSERT_TRUE(GetTestModules(&test_array_, &test_packed_array_));

    // Setup temp test dir.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    // Store full path to test file (without creating it yet).
    base::FilePath path = scoped_temp_dir_.GetPath();
    path = path.Append(kTestBlFileName);
    bl_test_file_path_ = std::move(path.value());

    // Override the file paths in the live code for testing.
    OverrideFilePathForTesting(bl_test_file_path_);
  }

  void TearDown() override { DeinitFromFile(); }

  void CreateTestFile() {
    base::File file(base::FilePath(bl_test_file_path_),
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                        base::File::FLAG_SHARE_DELETE |
                        base::File::FLAG_DELETE_ON_CLOSE);
    ASSERT_TRUE(file.IsValid());

    // Write content {metadata}{array_of_modules}.
    PackedListMetadata meta = {
        kInitialVersion, static_cast<uint32_t>(test_packed_array_.size())};
    ASSERT_EQ(file.Write(0, reinterpret_cast<const char*>(&meta), sizeof(meta)),
              static_cast<int>(sizeof(meta)));
    int size =
        static_cast<int>(test_packed_array_.size() * sizeof(PackedListModule));
    ASSERT_EQ(
        file.Write(sizeof(PackedListMetadata),
                   reinterpret_cast<const char*>(test_packed_array_.data()),
                   size),
        size);

    // Leave file handle open for DELETE_ON_CLOSE.
    bl_file_ = std::move(file);
  }

  const base::string16& GetBlTestFilePath() { return bl_test_file_path_; }

  base::File* GetBlFile() { return &bl_file_; }

  const std::vector<TestModule>& GetTestArray() { return test_array_; }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::File bl_file_;
  base::string16 bl_test_file_path_;
  std::vector<TestModule> test_array_;
  std::vector<PackedListModule> test_packed_array_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyFileTest);
};

//------------------------------------------------------------------------------
// Third-party file tests
//------------------------------------------------------------------------------

// Test successful initialization and module lookup.
TEST_F(ThirdPartyFileTest, Success) {
  // Create blacklist data file.
  ASSERT_NO_FATAL_FAILURE(CreateTestFile());

  // Init.
  ASSERT_EQ(InitFromFile(), ThirdPartyStatus::kSuccess);

  // Test matching.
  for (const auto& test_module : GetTestArray()) {
    elf_sha1::Digest name_hash = elf_sha1::SHA1HashString(test_module.basename);
    elf_sha1::Digest fingerprint_hash = elf_sha1::SHA1HashString(
        GetFingerprintString(test_module.timedatestamp, test_module.imagesize));
    EXPECT_TRUE(IsModuleListed(name_hash, fingerprint_hash));
  }

  // Test a failure to match.
  elf_sha1::Digest name_hash = elf_sha1::SHA1HashString("booya.dll");
  elf_sha1::Digest fingerprint_hash =
      elf_sha1::SHA1HashString(GetFingerprintString(0x12345678, 1337));
  EXPECT_FALSE(IsModuleListed(name_hash, fingerprint_hash));
}

// Test successful initialization with no packed files.
TEST_F(ThirdPartyFileTest, NoFiles) {
  // kFileNotFound is a non-fatal status code.
  ASSERT_EQ(InitFromFile(), ThirdPartyStatus::kFileNotFound);

  elf_sha1::Digest name_hash = elf_sha1::SHA1HashString("booya.dll");
  elf_sha1::Digest fingerprint_hash =
      elf_sha1::SHA1HashString(GetFingerprintString(0x12345678, 1337));
  EXPECT_FALSE(IsModuleListed(name_hash, fingerprint_hash));
}

TEST_F(ThirdPartyFileTest, CorruptFile) {
  ASSERT_NO_FATAL_FAILURE(CreateTestFile());

  base::File* file = GetBlFile();
  ASSERT_TRUE(file->IsValid());

  // 1) Not enough data for array size
  PackedListMetadata meta = {kCurrent, static_cast<uint32_t>(50)};
  ASSERT_EQ(file->Write(0, reinterpret_cast<const char*>(&meta), sizeof(meta)),
            static_cast<int>(sizeof(meta)));
  EXPECT_EQ(InitFromFile(), ThirdPartyStatus::kFileArrayReadFailure);

  // 2) Corrupt data or just unsupported metadata version.
  meta = {kUnsupported, static_cast<uint32_t>(50)};
  ASSERT_EQ(file->Write(0, reinterpret_cast<const char*>(&meta), sizeof(meta)),
            static_cast<int>(sizeof(meta)));
  EXPECT_EQ(InitFromFile(), ThirdPartyStatus::kFileInvalidFormatVersion);

  // 3) Not enough data for metadata.
  meta = {kCurrent, static_cast<uint32_t>(10)};
  ASSERT_EQ(
      file->Write(0, reinterpret_cast<const char*>(&meta), sizeof(meta) / 2),
      static_cast<int>(sizeof(meta) / 2));
  ASSERT_TRUE(file->SetLength(sizeof(meta) / 2));
  EXPECT_EQ(InitFromFile(), ThirdPartyStatus::kFileMetadataReadFailure);
}

// Test successful initialization, getting the file path from registry.
TEST_F(ThirdPartyFileTest, SuccessFromRegistry) {
  // 1. Enable reg override for test net.
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(RegRedirect(nt::HKCU, &override_manager));

  // 2. Add a sample ThirdParty subkey and value, which would be created by
  //    chrome.dll.
  ASSERT_TRUE(CreateRegistryKeyValue(GetBlTestFilePath()));

  // 3. Drop a blacklist to the expected path.
  ASSERT_NO_FATAL_FAILURE(CreateTestFile());

  // Clear override file path so that initialization goes to registry.
  OverrideFilePathForTesting(std::wstring());

  // 4. Run the test.
  EXPECT_EQ(InitFromFile(), ThirdPartyStatus::kSuccess);

  // 5. Disable reg override.
  ASSERT_NO_FATAL_FAILURE(CancelRegRedirect(nt::HKCU));
}

}  // namespace
}  // namespace third_party_dlls
