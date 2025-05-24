// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_file_host.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "components/nacl/browser/test_nacl_browser_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using nacl_file_host::PnaclCanOpenFile;

class FileHostTestNaClBrowserDelegate : public TestNaClBrowserDelegate {
 public:
  FileHostTestNaClBrowserDelegate() = default;

  bool GetPnaclDirectory(base::FilePath* pnacl_dir) override {
    *pnacl_dir = pnacl_path_;
    return true;
  }

  void SetPnaclDirectory(const base::FilePath& pnacl_dir) {
    pnacl_path_ = pnacl_dir;
  }

 private:
  base::FilePath pnacl_path_;
};

class NaClFileHostTest : public testing::Test {
 public:
  NaClFileHostTest(const NaClFileHostTest&) = delete;
  NaClFileHostTest& operator=(const NaClFileHostTest&) = delete;

 protected:
  NaClFileHostTest();
  ~NaClFileHostTest() override;

  void SetUp() override {
    nacl_browser_delegate_ = new FileHostTestNaClBrowserDelegate;
    nacl::NaClBrowser::SetDelegate(
        base::WrapUnique(nacl_browser_delegate_.get()));
  }

  void TearDown() override {
    nacl_browser_delegate_ = nullptr;
    nacl::NaClBrowser::ClearAndDeleteDelegate();
  }

  FileHostTestNaClBrowserDelegate* nacl_browser_delegate() {
    return nacl_browser_delegate_;
  }

 private:
  raw_ptr<FileHostTestNaClBrowserDelegate> nacl_browser_delegate_;
};

NaClFileHostTest::NaClFileHostTest() : nacl_browser_delegate_(nullptr) {}

NaClFileHostTest::~NaClFileHostTest() {
}

// Try to pass a few funny filenames with a dummy PNaCl directory set.
TEST_F(NaClFileHostTest, TestFilenamesWithPnaclPath) {
  base::ScopedTempDir scoped_tmp_dir;
  ASSERT_TRUE(scoped_tmp_dir.CreateUniqueTempDir());

  base::FilePath kTestPnaclPath = scoped_tmp_dir.GetPath();

  nacl_browser_delegate()->SetPnaclDirectory(kTestPnaclPath);
  ASSERT_TRUE(nacl::NaClBrowser::GetDelegate()->GetPnaclDirectory(
      &kTestPnaclPath));

  // Check allowed strings, and check that the expected prefix is added.
  base::FilePath out_path;
  EXPECT_TRUE(PnaclCanOpenFile("pnacl_json", &out_path));
  base::FilePath expected_path = kTestPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_pnacl_json"));
  EXPECT_EQ(expected_path, out_path);

  EXPECT_TRUE(PnaclCanOpenFile("x86_32_llc", &out_path));
  expected_path = kTestPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_x86_32_llc"));
  EXPECT_EQ(expected_path, out_path);

  // Check character ranges.
  EXPECT_FALSE(PnaclCanOpenFile(".xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x/chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("\\xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x\\chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("CAPS", &out_path));
  const char non_ascii[] = "\xff\xfe";
  EXPECT_FALSE(PnaclCanOpenFile(non_ascii, &out_path));

  // Check file length restriction.
  EXPECT_FALSE(PnaclCanOpenFile("thisstringisactuallywaaaaaaaaaaaaaaaaaaaaaaaa"
                                "toolongwaytoolongwaaaaayyyyytoooooooooooooooo"
                                "looooooooong",
                                &out_path));

  // Other bad files.
  EXPECT_FALSE(PnaclCanOpenFile(std::string(), &out_path));
  EXPECT_FALSE(PnaclCanOpenFile(".", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("..", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("../llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/bin/sh", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME/.bashrc", &out_path));
}
