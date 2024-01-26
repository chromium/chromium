// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_list_filter.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Typedef for convenience.
using ModuleInfo = std::pair<ModuleInfoKey, ModuleInfoData>;

std::string GetCodeId(uint32_t module_time_date_stamp, uint32_t module_size) {
  return base::StringPrintf("%08X%x", module_time_date_stamp, module_size);
}

// Helper class to build and serialize a ModuleList.
class ModuleListBuilder {
 public:
  explicit ModuleListBuilder(const base::FilePath& module_list_path)
      : module_list_path_(module_list_path) {
    // Include an empty blocklist and allowlist.
    module_list_.mutable_blocklist();
    module_list_.mutable_allowlist();
  }

  ModuleListBuilder(const ModuleListBuilder&) = delete;
  ModuleListBuilder& operator=(const ModuleListBuilder&) = delete;

  // Adds a module to the allowlist.
  void AddAllowlistedModule(std::optional<std::u16string> basename,
                            std::optional<std::string> code_id) {
    CHECK(basename.has_value() || code_id.has_value());

    chrome::conflicts::ModuleGroup* module_group =
        module_list_.mutable_allowlist()->add_module_groups();

    chrome::conflicts::Module* module = module_group->add_modules();

    if (basename.has_value()) {
      module->set_basename_hash(base::SHA1HashString(
          base::UTF16ToUTF8(base::i18n::ToLower(basename.value()))));
    }

    if (code_id.has_value())
      module->set_code_id_hash(base::SHA1HashString(code_id.value()));
  }

  // Adds a module to the allowlist. Used when both the Code ID and the basename
  // must be set.
  void AddAllowlistedModule(const ModuleInfoKey& module_key,
                            const ModuleInfoData& module_data) {
    AddAllowlistedModule(
        module_data.inspection_result->basename,
        GetCodeId(module_key.module_time_date_stamp, module_key.module_size));
  }

  // Adds a module to the blocklist.
  void AddBlocklistedModule(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data,
      bool allow_load_value,
      chrome::conflicts::BlocklistMessageType message_type,
      const std::string& message_url) {
    chrome::conflicts::BlocklistModuleGroup* module_group =
        module_list_.mutable_blocklist()->add_module_groups();

    chrome::conflicts::BlocklistAction* blocklist_action =
        module_group->mutable_action();
    blocklist_action->set_allow_load(true);
    blocklist_action->set_message_type(message_type);
    blocklist_action->set_message_url(message_url);

    chrome::conflicts::Module* module =
        module_group->mutable_modules()->add_modules();

    module->set_basename_hash(base::SHA1HashString(base::UTF16ToUTF8(
        base::i18n::ToLower(module_data.inspection_result->basename))));

    module->set_code_id_hash(base::SHA1HashString(
        GetCodeId(module_key.module_time_date_stamp, module_key.module_size)));
  }

  // Serializes the |module_list_| to |module_list_path_|. Returns true on
  // success.
  bool Finalize() {
    std::string contents;
    return module_list_.SerializeToString(&contents) &&
           base::WriteFile(module_list_path_, contents);
  }

 private:
  const base::FilePath module_list_path_;

  chrome::conflicts::ModuleList module_list_;
};

// Creates a pair of ModuleInfoKey and ModuleInfoData with the necessary
// information to call in IsModuleAllowlisted().
ModuleInfo CreateModuleInfo(const base::FilePath& module_path,
                            uint32_t module_size,
                            uint32_t module_time_date_stamp) {
  ModuleInfo result(
      std::piecewise_construct,
      std::forward_as_tuple(module_path, module_size, module_time_date_stamp),
      std::forward_as_tuple());

  result.second.inspection_result =
      std::make_optional<ModuleInspectionResult>();
  result.second.inspection_result->basename =
      module_path.BaseName().AsUTF16Unsafe();

  return result;
}

constexpr wchar_t kDllPath1[] = L"c:\\path\\to\\module.dll";
constexpr wchar_t kDllPath2[] = L"c:\\some\\shellextension.dll";

}  // namespace

class ModuleListFilterTest : public ::testing::Test {
 public:
  ModuleListFilterTest(const ModuleListFilterTest&) = delete;
  ModuleListFilterTest& operator=(const ModuleListFilterTest&) = delete;

 protected:
  ModuleListFilterTest()
      : dll1_(kDllPath1),
        dll2_(kDllPath2),
        module_list_filter_(base::MakeRefCounted<ModuleListFilter>()) {}

  ~ModuleListFilterTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    module_list_path_ = scoped_temp_dir_.GetPath().Append(L"ModuleList.bin");
  }

  const base::FilePath& module_list_path() { return module_list_path_; }
  ModuleListFilter& module_list_filter() { return *module_list_filter_; }

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath module_list_path_;

  scoped_refptr<ModuleListFilter> module_list_filter_;
};

TEST_F(ModuleListFilterTest, IsAllowlistedStringPieceVersion) {
  std::u16string basename = u"basename.dll";  // Must be lowercase.
  std::string code_id = GetCodeId(12u, 32u);

  ModuleListBuilder module_list_builder(module_list_path());
  module_list_builder.AddAllowlistedModule(basename, code_id);
  ASSERT_TRUE(module_list_builder.Finalize());

  ASSERT_TRUE(module_list_filter().Initialize(module_list_path()));

  // Calculate hashes.
  std::string basename_hash = base::SHA1HashString(base::UTF16ToUTF8(basename));
  std::string code_id_hash = base::SHA1HashString(code_id);

  EXPECT_TRUE(module_list_filter().IsAllowlisted(basename_hash, code_id_hash));
}

TEST_F(ModuleListFilterTest, AllowlistedModules) {
  ModuleInfo module_1 = CreateModuleInfo(dll1_, 0123, 4567);
  ModuleInfo module_2 = CreateModuleInfo(dll2_, 7654, 3210);

  ModuleListBuilder module_list_builder(module_list_path());
  module_list_builder.AddAllowlistedModule(module_1.first, module_1.second);
  ASSERT_TRUE(module_list_builder.Finalize());

  ASSERT_TRUE(module_list_filter().Initialize(module_list_path()));

  EXPECT_TRUE(
      module_list_filter().IsAllowlisted(module_1.first, module_1.second));
  EXPECT_FALSE(
      module_list_filter().IsAllowlisted(module_2.first, module_2.second));
}

TEST_F(ModuleListFilterTest, BlocklistedModules) {
  const char kFurtherInfoURL[] = "http://www.further-info.com";

  ModuleInfo module_1 = CreateModuleInfo(dll1_, 0123, 4567);
  ModuleInfo module_2 = CreateModuleInfo(dll2_, 7654, 3210);

  ModuleListBuilder module_list_builder(module_list_path());
  module_list_builder.AddBlocklistedModule(
      module_1.first, module_1.second, true,
      chrome::conflicts::BlocklistMessageType::FURTHER_INFORMATION,
      kFurtherInfoURL);
  ASSERT_TRUE(module_list_builder.Finalize());

  ASSERT_TRUE(module_list_filter().Initialize(module_list_path()));

  std::unique_ptr<chrome::conflicts::BlocklistAction> blocklist_action =
      module_list_filter().IsBlocklisted(module_1.first, module_1.second);
  ASSERT_TRUE(blocklist_action);
  EXPECT_TRUE(blocklist_action->allow_load());
  EXPECT_EQ(chrome::conflicts::BlocklistMessageType::FURTHER_INFORMATION,
            blocklist_action->message_type());
  EXPECT_EQ(kFurtherInfoURL, blocklist_action->message_url());
  EXPECT_FALSE(
      module_list_filter().IsBlocklisted(module_2.first, module_2.second));
}

TEST_F(ModuleListFilterTest, BasenameOnly) {
  ModuleInfo original =
      CreateModuleInfo(base::FilePath(L"c:\\path\\basename.dll"), 1111, 0001);
  ModuleInfo same_basename = CreateModuleInfo(
      base::FilePath(L"c:\\wrong_path\\basename.dll"), 2222, 0002);
  ModuleInfo same_path = CreateModuleInfo(
      base::FilePath(L"c:\\path\\wrong_basename.dll"), 3333, 0003);
  ModuleInfo same_code_id = CreateModuleInfo(
      base::FilePath(L"c:\\wrong_path\\wrong_basename.dll"), 1111, 0001);

  ModuleListBuilder module_list_builder(module_list_path());
  module_list_builder.AddAllowlistedModule(
      original.second.inspection_result->basename, std::nullopt);
  ASSERT_TRUE(module_list_builder.Finalize());

  ASSERT_TRUE(module_list_filter().Initialize(module_list_path()));

  EXPECT_TRUE(
      module_list_filter().IsAllowlisted(original.first, original.second));
  EXPECT_TRUE(module_list_filter().IsAllowlisted(same_basename.first,
                                                 same_basename.second));
  EXPECT_FALSE(
      module_list_filter().IsAllowlisted(same_path.first, same_path.second));
  EXPECT_FALSE(module_list_filter().IsAllowlisted(same_code_id.first,
                                                  same_code_id.second));
}

TEST_F(ModuleListFilterTest, CodeIdOnly) {
  ModuleInfo original =
      CreateModuleInfo(base::FilePath(L"c:\\path\\basename.dll"), 1111, 0001);
  ModuleInfo same_basename = CreateModuleInfo(
      base::FilePath(L"c:\\wrong_path\\basename.dll"), 2222, 0002);
  ModuleInfo same_code_id = CreateModuleInfo(
      base::FilePath(L"c:\\wrong_path\\wrong_basename.dll"), 1111, 0001);

  ModuleListBuilder module_list_builder(module_list_path());
  module_list_builder.AddAllowlistedModule(
      std::nullopt, GetCodeId(original.first.module_time_date_stamp,
                              original.first.module_size));
  ASSERT_TRUE(module_list_builder.Finalize());

  ASSERT_TRUE(module_list_filter().Initialize(module_list_path()));

  EXPECT_TRUE(
      module_list_filter().IsAllowlisted(original.first, original.second));
  EXPECT_FALSE(module_list_filter().IsAllowlisted(same_basename.first,
                                                  same_basename.second));
  EXPECT_TRUE(module_list_filter().IsAllowlisted(same_code_id.first,
                                                 same_code_id.second));
}
