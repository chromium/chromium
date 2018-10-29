// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "chrome/test/ppapi/ppapi_test_select_file_dialog_factory.h"
#include "components/download/quarantine/test_support.h"
#include "ppapi/shared_impl/test_utils.h"

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "components/safe_browsing/db/test_database_manager.h"

using safe_browsing::DownloadProtectionService;
using safe_browsing::SafeBrowsingService;
#endif

namespace {

class PPAPIFileChooserTest : public OutOfProcessPPAPITest {};

#if defined(FULL_SAFE_BROWSING)

struct SafeBrowsingTestConfiguration {
  std::map<base::FilePath::StringType, safe_browsing::DownloadCheckResult>
      result_map;
  safe_browsing::DownloadCheckResult default_result =
      safe_browsing::DownloadCheckResult::SAFE;
};

class FakeDownloadProtectionService : public DownloadProtectionService {
 public:
  explicit FakeDownloadProtectionService(
      const SafeBrowsingTestConfiguration* test_config)
      : DownloadProtectionService(nullptr), test_configuration_(test_config) {}

  void CheckPPAPIDownloadRequest(
      const GURL& requestor_url,
      const GURL& initiating_frame_url_unused,
      content::WebContents* web_contents_unused,
      const base::FilePath& default_file_path,
      const std::vector<base::FilePath::StringType>& alternate_extensions,
      Profile* /* profile */,
      const safe_browsing::CheckDownloadCallback& callback) override {
    const auto iter =
        test_configuration_->result_map.find(default_file_path.Extension());
    if (iter != test_configuration_->result_map.end()) {
      callback.Run(iter->second);
      return;
    }

    for (const auto extension : alternate_extensions) {
      EXPECT_EQ(base::FilePath::kExtensionSeparator, extension[0]);
      const auto iter = test_configuration_->result_map.find(extension);
      if (iter != test_configuration_->result_map.end()) {
        callback.Run(iter->second);
        return;
      }
    }

    callback.Run(test_configuration_->default_result);
  }

 private:
  const SafeBrowsingTestConfiguration* test_configuration_;
};

class TestSafeBrowsingService : public safe_browsing::TestSafeBrowsingService {
 public:
  explicit TestSafeBrowsingService(const SafeBrowsingTestConfiguration* config)
      : test_configuration_(config) {
    services_delegate_ =
        safe_browsing::ServicesDelegate::CreateForTest(this, this);
  }

 private:
  // safe_browsing::ServicesDelegate::ServicesCreator
  bool CanCreateDownloadProtectionService() override { return true; }
  DownloadProtectionService* CreateDownloadProtectionService() override {
    return new FakeDownloadProtectionService(test_configuration_);
  }

  const SafeBrowsingTestConfiguration* test_configuration_;
};

class TestSafeBrowsingServiceFactory
    : public safe_browsing::SafeBrowsingServiceFactory {
 public:
  explicit TestSafeBrowsingServiceFactory(
      const SafeBrowsingTestConfiguration* config)
      : test_configuration_(config) {}

  SafeBrowsingService* CreateSafeBrowsingService() override {
    SafeBrowsingService* service =
        new TestSafeBrowsingService(test_configuration_);
    return service;
  }

 private:
  const SafeBrowsingTestConfiguration* test_configuration_;
};

class PPAPIFileChooserTestWithSBService : public PPAPIFileChooserTest {
 public:
  PPAPIFileChooserTestWithSBService()
      : safe_browsing_service_factory_(&safe_browsing_test_configuration_) {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(&safe_browsing_service_factory_);
    PPAPIFileChooserTest::SetUp();
  }
  void TearDown() override {
    PPAPIFileChooserTest::TearDown();
    SafeBrowsingService::RegisterFactory(nullptr);
  }

 protected:
  SafeBrowsingTestConfiguration safe_browsing_test_configuration_;

 private:
  TestSafeBrowsingServiceFactory safe_browsing_service_factory_;
};

#endif

}  // namespace

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_Open_Success) {
  const char kContents[] = "Hello from browser";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath existing_filename = temp_dir.GetPath().AppendASCII("foo");
  ASSERT_EQ(
      static_cast<int>(sizeof(kContents) - 1),
      base::WriteFile(existing_filename, kContents, sizeof(kContents) - 1));

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(existing_filename, existing_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST, file_info_list);
  RunTestViaHTTP("FileChooser_OpenSimple");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_Open_Cancel) {
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::CANCEL,
      PPAPITestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_OpenCancel");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_SaveAs_Success) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.GetPath().AppendASCII("foo");

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsSafeDefaultName");
  ASSERT_TRUE(base::PathExists(suggested_filename));
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_SafeDefaultName) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.GetPath().AppendASCII("foo");

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsSafeDefaultName");
  base::FilePath actual_filename =
      temp_dir.GetPath().AppendASCII("innocuous.txt");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_UnsafeDefaultName) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.GetPath().AppendASCII("foo");

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsUnsafeDefaultName");
  base::FilePath actual_filename =
      temp_dir.GetPath().AppendASCII("unsafe.txt_");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_SaveAs_Cancel) {
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::CANCEL,
      PPAPITestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsCancel");
}

#if defined(OS_WIN) || defined(OS_LINUX)
// On Windows, tests that a file downloaded via PPAPI FileChooser API has the
// mark-of-the-web. The PPAPI FileChooser implementation invokes QuarantineFile
// in order to mark the file as being downloaded from the web as soon as the
// file is created. This MOTW prevents the file being opened without due
// security warnings if the file is executable.
IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_Quarantine) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.GetPath().AppendASCII("foo");

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableAllowed");
  base::FilePath actual_filename =
      temp_dir.GetPath().AppendASCII("dangerous.exe");

  ASSERT_TRUE(base::PathExists(actual_filename));
  EXPECT_TRUE(download::IsFileQuarantined(actual_filename, GURL(), GURL()));
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(FULL_SAFE_BROWSING)
// These tests only make sense when SafeBrowsing is enabled. They verify
// that files written via the FileChooser_Trusted API are properly passed
// through Safe Browsing.

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTestWithSBService,
                       FileChooser_SaveAs_DangerousExecutable_Allowed) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  safe_browsing_test_configuration_.default_result =
      safe_browsing::DownloadCheckResult::DANGEROUS;
  safe_browsing_test_configuration_.result_map.insert(
      std::make_pair(base::FilePath::StringType(FILE_PATH_LITERAL(".exe")),
                     safe_browsing::DownloadCheckResult::SAFE));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.GetPath().AppendASCII("foo");

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableAllowed");
  base::FilePath actual_filename =
      temp_dir.GetPath().AppendASCII("dangerous.exe");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTestWithSBService,
                       FileChooser_SaveAs_DangerousExecutable_Disallowed) {
  safe_browsing_test_configuration_.default_result =
      safe_browsing::DownloadCheckResult::SAFE;
  safe_browsing_test_configuration_.result_map.insert(
      std::make_pair(base::FilePath::StringType(FILE_PATH_LITERAL(".exe")),
                     safe_browsing::DownloadCheckResult::DANGEROUS));

  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::NOT_REACHED,
      PPAPITestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableDisallowed");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTestWithSBService,
                       FileChooser_SaveAs_DangerousExtensionList_Disallowed) {
  safe_browsing_test_configuration_.default_result =
      safe_browsing::DownloadCheckResult::SAFE;
  safe_browsing_test_configuration_.result_map.insert(
      std::make_pair(base::FilePath::StringType(FILE_PATH_LITERAL(".exe")),
                     safe_browsing::DownloadCheckResult::DANGEROUS));

  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::NOT_REACHED,
      PPAPITestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsDangerousExtensionListDisallowed");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTestWithSBService,
                       FileChooser_Open_NotBlockedBySafeBrowsing) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const char kContents[] = "Hello from browser";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath existing_filename = temp_dir.GetPath().AppendASCII("foo");
  ASSERT_EQ(
      static_cast<int>(sizeof(kContents) - 1),
      base::WriteFile(existing_filename, kContents, sizeof(kContents) - 1));

  safe_browsing_test_configuration_.default_result =
      safe_browsing::DownloadCheckResult::DANGEROUS;

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(existing_filename, existing_filename));
  PPAPITestSelectFileDialogFactory test_dialog_factory(
      PPAPITestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST, file_info_list);
  RunTestViaHTTP("FileChooser_OpenSimple");
}

#endif  // FULL_SAFE_BROWSING
