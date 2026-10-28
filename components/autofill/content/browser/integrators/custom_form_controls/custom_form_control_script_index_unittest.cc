// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_index.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::Optional;

// Tests that FindScript() returns the script of the first entry whose host and
// `url_filter` match the URL.
TEST(CustomFormControlScriptIndexTest, FindScriptMatchesHostAndPathForRequest) {
  CustomFormControlScriptIndex index;
  EXPECT_EQ(index.FindScript(GURL("https://example.com/")), std::nullopt);
  index.SetEntries({
      {.host = "shop.example.com",
       .url_filter = "",
       .script = "subdomain_script"},
      {.host = "Example.COM",
       .url_filter = "/checkout",
       .script = "checkout_script"},
      {.host = "example.com",
       .url_filter = R"(index\.php\?cl=user)",
       .script = "query_script"},
      {.host = "example.com", .url_filter = "", .script = "fallback_script"},
  });

  // Subdomain entry matches shop.example.com and its subdomains, not parent.
  EXPECT_THAT(index.FindScript(GURL("https://shop.example.com/cart")),
              Optional(std::string("subdomain_script")));
  EXPECT_THAT(index.FindScript(GURL("https://us.shop.example.com/cart")),
              Optional(std::string("subdomain_script")));

  // Matches normalized host, subdomains, custom ports, and path/query filter.
  EXPECT_THAT(index.FindScript(GURL("https://www.example.com:8443/checkout")),
              Optional(std::string("checkout_script")));
  EXPECT_THAT(index.FindScript(GURL("http://a.b.example.com/shop/checkout")),
              Optional(std::string("checkout_script")));
  EXPECT_THAT(
      index.FindScript(GURL("https://example.com/index.php?cl=user&step=1")),
      Optional(std::string("query_script")));
  EXPECT_THAT(index.FindScript(GURL("https://example.com/signup")),
              Optional(std::string("fallback_script")));

  // URL fragment (#...) is excluded from `url_filter` matching.
  EXPECT_THAT(index.FindScript(GURL("https://example.com/login#/checkout")),
              Optional(std::string("fallback_script")));

  // Non-matching hosts and non-HTTP(S) schemes return nullopt.
  EXPECT_EQ(index.FindScript(GURL("https://notexample.com/checkout")),
            std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("file:///checkout")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("about:blank")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL()), std::nullopt);
}

// Tests that SetEntries() drops entries with an invalid host, `url_filter` or
// script, and keeps the valid ones.
TEST(CustomFormControlScriptIndexTest, SetEntriesDropsInvalidEntries) {
  CustomFormControlScriptIndex index;
  index.SetEntries({
      // Invalid hosts:
      {.host = "", .url_filter = "", .script = "empty_host"},
      {.host = "com", .url_filter = "", .script = "public_suffix"},
      {.host = "co.uk", .url_filter = "", .script = "public_suffix_2"},
      {.host = "github.io", .url_filter = "", .script = "private_registry"},
      {.host = ".example.com", .url_filter = "", .script = "leading_dot"},
      {.host = "example.com.", .url_filter = "", .script = "trailing_dot"},
      {.host = "example.com:443", .url_filter = "", .script = "with_port"},
      {.host = "example.com/path", .url_filter = "", .script = "with_path"},
      // Invalid `url_filter`:
      {.host = "example.com", .url_filter = "[", .script = "bad_regex"},
      // Invalid scripts:
      {.host = "example.com", .url_filter = "", .script = ""},
      {.host = "example.com",
       .url_filter = "",
       .script = std::string(kMaxCustomFormControlScriptSizeBytes + 1, 'x')},
      // Valid entries:
      {.host = "foo.github.io", .url_filter = "", .script = "private_ok"},
      {.host = "example.com", .url_filter = "", .script = "valid_script"},
  });
  EXPECT_THAT(index.FindScript(GURL("https://example.com/")),
              Optional(std::string("valid_script")));
  EXPECT_THAT(index.FindScript(GURL("https://foo.github.io/")),
              Optional(std::string("private_ok")));
  EXPECT_EQ(index.FindScript(GURL("https://bar.github.io/")), std::nullopt);
  EXPECT_EQ(index.FindScript(GURL("https://other.com/")), std::nullopt);
}

// Tests that SetEntries() replaces all previously set entries.
TEST(CustomFormControlScriptIndexTest, SetEntriesReplacesPreviousEntries) {
  CustomFormControlScriptIndex index;
  index.SetEntries({
      {.host = "example.com", .url_filter = "", .script = "old_script"},
      {.host = "other.com", .url_filter = "", .script = "other_script"},
  });
  index.SetEntries(
      {{.host = "example.com", .url_filter = "", .script = "new_script"}});
  EXPECT_THAT(index.FindScript(GURL("https://example.com/")),
              Optional(std::string("new_script")));
  EXPECT_EQ(index.FindScript(GURL("https://other.com/")), std::nullopt);
}

class CustomFormControlScriptIndexDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Writes `content` to `path`, relative to the temporary directory.
  void WriteFile(std::string_view path, std::string_view content) {
    const base::FilePath file_path =
        temp_dir_.GetPath().Append(base::FilePath::FromUTF8Unsafe(path));
    ASSERT_TRUE(base::CreateDirectory(file_path.DirName()));
    ASSERT_TRUE(base::WriteFile(file_path, content));
  }

  // Runs `InitializeFromDirectory()` and waits until it has finished.
  void InitializeFromDirectory() {
    index_.InitializeFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(base::test::RunUntil([&]() { return index_.is_initialized(); }))
        << "Timed out waiting for the scripts to be read";
  }

  std::optional<std::string> FindScript(std::string_view url) {
    return index_.FindScript(GURL(url));
  }

  CustomFormControlScriptIndex& index() { return index_; }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  CustomFormControlScriptIndex index_;
};

TEST_F(CustomFormControlScriptIndexDirectoryTest,
       LoadsValidEntriesAndSkipsInvalidOnes) {
  WriteFile(
      "index.json",
      "["
      // Valid entries:
      R"({"host": "Example.COM", "url_filter": "/buy",)"
      R"( "script_path": "a.js"},)"
      R"({"host": "nofilter.com", "script_path": "b.js"},)"
      // - `script_path` in a subdirectory.
      R"({"host": "subdir.com", "script_path": "dir/c.js"},)"
      // Invalid entries:
      // - `script_path` outside the directory.
      R"({"host": "traversal.com", "script_path": "../a.js"},)"
      R"({"host": "traversal2.com", "script_path": "dir/../../a.js"},)"
      R"({"host": "absolute.com", "script_path": "/tmp/a.js"},)"
      // - Script file that doesn't exist.
      R"({"host": "missing.com", "script_path": "missing.js"},)"
      // - Non-string fields.
      R"({"host": "badfilter.com", "url_filter": 1, "script_path": "a.js"},)"
      R"({"host": 1, "script_path": "a.js"},)"
      // - Missing `script_path`.
      R"({"host": "noname.com"},)"
      // - Not an object.
      R"("not an object")"
      "]");
  WriteFile("a.js", "script_a");
  WriteFile("b.js", "script_b");
  WriteFile("dir/c.js", "script_c");
  InitializeFromDirectory();

  EXPECT_THAT(FindScript("https://shop.example.com/buy"),
              Optional(std::string("script_a")));
  EXPECT_EQ(FindScript("https://shop.example.com/"), std::nullopt);
  EXPECT_THAT(FindScript("https://nofilter.com/any/page"),
              Optional(std::string("script_b")));
  EXPECT_THAT(FindScript("https://subdir.com/"),
              Optional(std::string("script_c")));
  for (const char* url : {"https://traversal.com/", "https://traversal2.com/",
                          "https://absolute.com/", "https://missing.com/",
                          "https://badfilter.com/", "https://noname.com/"}) {
    SCOPED_TRACE(url);
    EXPECT_EQ(FindScript(url), std::nullopt);
  }
}

// Tests that a `script_path` with an absolute path is skipped, even if it
// points to a file inside the directory.
TEST_F(CustomFormControlScriptIndexDirectoryTest, SkipsAbsoluteScriptPath) {
  WriteFile("a.js", "script_a");
  base::DictValue relative_entry;
  relative_entry.Set("host", "relative.com");
  relative_entry.Set("script_path", "a.js");
  base::DictValue absolute_entry;
  absolute_entry.Set("host", "absolute.com");
  absolute_entry.Set("script_path",
                     temp_dir().AppendASCII("a.js").AsUTF8Unsafe());
  base::ListValue list;
  list.Append(std::move(relative_entry));
  list.Append(std::move(absolute_entry));
  std::optional<std::string> json = base::WriteJson(list);
  ASSERT_TRUE(json);
  WriteFile("index.json", *json);
  InitializeFromDirectory();

  EXPECT_THAT(FindScript("https://relative.com/"),
              Optional(std::string("script_a")));
  EXPECT_EQ(FindScript("https://absolute.com/"), std::nullopt);
}

TEST_F(CustomFormControlScriptIndexDirectoryTest, EmptyListClearsEntries) {
  index().SetEntries({{.host = "example.org", .script = "old_script"}});
  WriteFile("index.json", "[]");
  InitializeFromDirectory();
  EXPECT_EQ(FindScript("https://example.org/"), std::nullopt);
}

TEST_F(CustomFormControlScriptIndexDirectoryTest,
       KeepsEntriesIfIndexIsMissingOrInvalid) {
  index().SetEntries({{.host = "example.org", .script = "old_script"}});
  InitializeFromDirectory();  // No index.json.
  EXPECT_THAT(FindScript("https://example.org/"),
              Optional(std::string("old_script")));

  WriteFile("ok.js", "script_body");
  for (const char* json :
       {"not json", R"({"host": "a.com", "script_path": "ok.js"})",
        R"([{"host": "a.com", "script_path": "ok.js"},])"}) {
    SCOPED_TRACE(json);
    WriteFile("index.json", json);
    InitializeFromDirectory();
    EXPECT_THAT(FindScript("https://example.org/"),
                Optional(std::string("old_script")));
  }
}

}  // namespace
}  // namespace autofill
