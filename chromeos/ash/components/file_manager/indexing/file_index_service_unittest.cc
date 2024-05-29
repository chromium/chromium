// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/query.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace ash::file_manager {
namespace {

GURL MakeLocalURL(const std::string& file_name) {
  return GURL(base::StrCat(
      {"filesystem:chrome://file-manager/external/Downloads-user123/",
       file_name}));
}

GURL MakeDriveURL(const std::string& file_name) {
  return GURL(base::StrCat(
      {"filesystem:chrome://file-manager/external/drivefs-987654321/",
       file_name}));
}

MATCHER_P(ContainsFiles, expected_files, "") {
  std::set<FileInfo> result_set;
  for (const Match& match : arg.matches) {
    result_set.emplace(match.file_info);
  }
  std::set<FileInfo> expected_set;
  for (const FileInfo& info : expected_files) {
    expected_set.emplace(info);
  }
  return expected_set == result_set;
}

class FileIndexServiceTest : public testing::Test {
 public:
  FileIndexServiceTest()
      : pinned_("label", u"pinned"),
        downloaded_("label", u"downloaded"),
        starred_("label", u"starred") {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scheme_registry_ = std::make_unique<url::ScopedSchemeRegistryForTests>();
    url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
    // These URLs must be created after scheme_registry_ has 'chrome' scheme
    // added to it. Otherwise, they are deemed invalid.
    foo_url_ = MakeLocalURL("foo.txt");
    bar_url_ = MakeLocalURL("bar.txt");
    CreateIndex();
  }

  void TearDown() override {
    DestroyIndex();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  // Convenience methods that convert asynchronous results to synchronous.
  void CreateIndex() {
    base::FilePath db_path =
        temp_dir_.GetPath().Append("FileIndexServiceTest.db");
    index_service_ = std::make_unique<FileIndexService>(db_path);
    ASSERT_EQ(Init(), OpResults::kSuccess);
  }

  void DestroyIndex() { index_service_.reset(); }

  OpResults Init() {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->Init(base::BindLambdaForTesting([&](OpResults results) {
      outcome = results;
      run_loop.Quit();
    }));
    run_loop.Run();
    return outcome;
  }

  SearchResults Search(const Query& query) {
    base::RunLoop run_loop;
    SearchResults outcome;
    index_service_->Search(
        query, base::BindLambdaForTesting([&](SearchResults results) {
          outcome.total_matches = results.total_matches;
          outcome.matches = results.matches;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults PutFileInfo(const FileInfo& file_info) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->PutFileInfo(
        file_info, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults SetTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->SetTerms(terms, url,
                             base::BindLambdaForTesting([&](OpResults results) {
                               outcome = results;
                               run_loop.Quit();
                             }));
    run_loop.Run();
    return outcome;
  }

  OpResults AddTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->AddTerms(terms, url,
                             base::BindLambdaForTesting([&](OpResults results) {
                               outcome = results;
                               run_loop.Quit();
                             }));
    run_loop.Run();
    return outcome;
  }

  OpResults RemoveTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->RemoveTerms(
        terms, url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults RemoveFile(const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->RemoveFile(
        url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults MoveFile(const GURL& old_url, const GURL& new_url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->MoveFile(old_url, new_url,
                             base::BindLambdaForTesting([&](OpResults results) {
                               outcome = results;
                               run_loop.Quit();
                             }));
    run_loop.Run();
    return outcome;
  }

  Term pinned_;
  Term downloaded_;
  Term starred_;
  GURL foo_url_;
  GURL bar_url_;
  std::unique_ptr<FileIndexService> index_service_;
  base::ScopedTempDir temp_dir_;
  // Allows registering the "chrome://" scheme, without depending on //content.
  std::unique_ptr<url::ScopedSchemeRegistryForTests> scheme_registry_;
  base::test::TaskEnvironment task_environment_;
};

typedef std::vector<FileInfo> FileInfoList;

TEST_F(FileIndexServiceTest, InitializeTwice) {
  ASSERT_EQ(Init(), OpResults::kSuccess);
}

TEST_F(FileIndexServiceTest, CreateDestroyCreate) {
  // Index is already created by SetUp(). Thus just destroy it and create.
  // it again.
  DestroyIndex();
  // TODO(b:327534824): Remove the sleep statement.
  base::PlatformThread::Sleep(base::Milliseconds(250));
  CreateIndex();
}

TEST_F(FileIndexServiceTest, EmptySearch) {
  // Empty query on an empty index.
  EXPECT_THAT(Search(Query({})), ContainsFiles(FileInfoList{}));

  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({pinned_}, file_info.file_url), OpResults::kSuccess);

  // Empty query on an non-empty index.
  EXPECT_THAT(Search(Query({})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, SimpleMatch) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({pinned_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, MultiTermMatch) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  // Label file_info as pinned and starred.
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({pinned_, starred_}, file_info.file_url),
            OpResults::kSuccess);

  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));

  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  EXPECT_THAT(Search(Query({pinned_, starred_})),
              ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, AddTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  // Label file_info as pinned and starred.
  EXPECT_EQ(SetTerms({downloaded_}, file_info.file_url), OpResults::kSuccess);

  // Can find by downloaded.
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  // Cannot find by starred.
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));

  EXPECT_EQ(AddTerms({starred_}, foo_url_), OpResults::kSuccess);
  // Can find by downloaded.
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  // And by starred.
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));
  // And by starred and downloaded.
  EXPECT_THAT(Search(Query({starred_, downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, ReplaceTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  // Start with the single label: downloaded.
  EXPECT_EQ(SetTerms({downloaded_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));

  // Just adding more labels: both downloaded and starred.
  EXPECT_EQ(SetTerms({downloaded_, starred_}, file_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  // Remove the original "downloaded" label.
  EXPECT_EQ(SetTerms({starred_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})), ContainsFiles(FileInfoList{}));
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  // Remove the "starred" label and add back "downloaded".
  EXPECT_EQ(SetTerms({downloaded_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList({})));
}

TEST_F(FileIndexServiceTest, SearchMultipleFiles) {
  FileInfo foo_file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(foo_file_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({downloaded_}, foo_file_info.file_url),
            OpResults::kSuccess);

  GURL bar_drive_url = MakeDriveURL("bar.txt");
  FileInfo bar_file_info(bar_drive_url, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(bar_file_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({downloaded_}, bar_file_info.file_url),
            OpResults::kSuccess);

  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_file_info, bar_file_info}));
}

TEST_F(FileIndexServiceTest, SearchByNonexistingTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);

  EXPECT_EQ(SetTerms({pinned_}, file_info.file_url), OpResults::kSuccess);

  EXPECT_THAT(Search(Query({downloaded_})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, EmptySetTermsIsInvalid) {
  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);

  // Insert into the index with pinned label.
  EXPECT_EQ(SetTerms({pinned_}, file_info.file_url), OpResults::kSuccess);
  // Verify that passing empty terms is disallowed.
  EXPECT_EQ(SetTerms({}, file_info.file_url), OpResults::kArgumentError);

  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, FieldSeparator) {
  Term colon_in_field("foo:", u"one");
  FileInfo foo_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);

  EXPECT_EQ(SetTerms({colon_in_field}, foo_info.file_url), OpResults::kSuccess);

  Term colon_in_text("foo", u":one");
  FileInfo bar_info(bar_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(bar_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({colon_in_text}, bar_info.file_url), OpResults::kSuccess);

  EXPECT_THAT(Search(Query({colon_in_field})),
              ContainsFiles(FileInfoList{foo_info}));
  EXPECT_THAT(Search(Query({colon_in_text})),
              ContainsFiles(FileInfoList{bar_info}));
}

TEST_F(FileIndexServiceTest, GlobalSearch) {
  // Setup: two files, one marked with the label:starred, the other with
  // content:starred. This simulates the case where identical tokens, "starred"
  // came from two different sources (labeling, and file content).
  const std::u16string text = u"starred";
  Term label_term("label", text);
  Term content_term("content", text);
  FileInfo labeled_info(foo_url_, 1024, base::Time());
  FileInfo content_info(bar_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(labeled_info), OpResults::kSuccess);
  EXPECT_EQ(PutFileInfo(content_info), OpResults::kSuccess);

  EXPECT_EQ(SetTerms({label_term}, labeled_info.file_url), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({content_term}, content_info.file_url),
            OpResults::kSuccess);

  // Searching with empty field name means global space search.
  EXPECT_THAT(Search(Query({Term("", text)})),
              ContainsFiles(FileInfoList{labeled_info, content_info}));
  // Searching with field name, gives us unique results.
  EXPECT_THAT(Search(Query({label_term})),
              ContainsFiles(FileInfoList{labeled_info}));
  EXPECT_THAT(Search(Query({content_term})),
              ContainsFiles(FileInfoList{content_info}));
}

TEST_F(FileIndexServiceTest, MixedSearch) {
  // Setup: two files, both starred, one labeled "tax", one containing the word
  // "tax" in its content.
  const std::u16string tax_text = u"tax";
  Term tax_content_term("content", tax_text);
  Term tax_label_term("label", tax_text);
  FileInfo tax_label_info(foo_url_, 1024, base::Time());
  FileInfo tax_content_info(bar_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(tax_label_info), OpResults::kSuccess);
  EXPECT_EQ(PutFileInfo(tax_content_info), OpResults::kSuccess);

  EXPECT_EQ(SetTerms({starred_, tax_content_term}, tax_content_info.file_url),
            OpResults::kSuccess);
  EXPECT_EQ(SetTerms({starred_, tax_label_term}, tax_label_info.file_url),
            OpResults::kSuccess);

  // Searching with "starred tax" should return both files.
  EXPECT_THAT(Search(Query({Term("", tax_text), Term("", u"starred")})),
              ContainsFiles(FileInfoList{tax_content_info, tax_label_info}));
  // Searching with with "label:starred content:tax" gives us just the file that
  // has "tax" in content.
  EXPECT_THAT(Search(Query({starred_, tax_content_term})),
              ContainsFiles(FileInfoList{tax_content_info}));
  // Searching with with "label:starred label:tax" gives us just the file that
  // has "tax" as a label.
  EXPECT_THAT(Search(Query({starred_, tax_label_term})),
              ContainsFiles(FileInfoList{tax_label_info}));
}

TEST_F(FileIndexServiceTest, MoveFile) {
  // Test 1: Move non-existing file.
  EXPECT_EQ(MoveFile(foo_url_, bar_url_), OpResults::kFileMissing);

  // Test 2: Move file to itself.
  FileInfo foo_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);
  EXPECT_EQ(MoveFile(foo_info.file_url, foo_info.file_url),
            OpResults::kSuccess);

  // Test 3: Move file onto existing file.
  FileInfo bar_info(bar_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(bar_info), OpResults::kSuccess);
  EXPECT_EQ(MoveFile(foo_info.file_url, bar_info.file_url),
            OpResults::kFileExists);

  // Test 4: Actually move the file to the new URL.
  // First setup terms and make sure we can find the file by those terms.
  EXPECT_EQ(SetTerms({starred_, downloaded_}, foo_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{foo_info}));
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_info}));

  // Now actually move the file and verify that we cannot match the old foo_info
  // but can match foo_info_new.
  GURL new_foo_url = MakeLocalURL("foo2.txt");
  FileInfo new_foo_info(new_foo_url, foo_info.size, foo_info.last_modified);
  EXPECT_EQ(MoveFile(foo_info.file_url, new_foo_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{new_foo_info}));
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{new_foo_info}));
  // We cannot directly check if a given URL exists in the system, but we
  // can try to update terms using old URL and expect a kFileMissing error.
  EXPECT_EQ(SetTerms({starred_}, foo_info.file_url), OpResults::kFileMissing);
}

TEST_F(FileIndexServiceTest, RemoveFile) {
  // Empty remove.
  FileInfo foo_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(RemoveFile(foo_info.file_url), OpResults::kSuccess);
  // Add foo_info to the index.
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({starred_}, foo_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{foo_info}));
  EXPECT_EQ(RemoveFile(foo_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, RemoveTerms) {
  FileInfo foo_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(RemoveTerms({}, foo_url_), OpResults::kSuccess);

  // Add terms for foo_info.
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);
  EXPECT_EQ(SetTerms({starred_, downloaded_}, foo_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{foo_info}));
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_info}));

  EXPECT_EQ(RemoveTerms({starred_}, foo_info.file_url), OpResults::kSuccess);

  EXPECT_TRUE(Search(Query({starred_})).matches.empty());
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_info}));

  // Remove more terms, including one that is no longer there.
  EXPECT_EQ(RemoveTerms({starred_, downloaded_}, foo_info.file_url),
            OpResults::kSuccess);

  EXPECT_TRUE(Search(Query({starred_})).matches.empty());
  EXPECT_TRUE(Search(Query({downloaded_})).matches.empty());
}

TEST_F(FileIndexServiceTest, AddOrSetBeforePut) {
  EXPECT_EQ(SetTerms({starred_}, foo_url_), OpResults::kFileMissing);
  EXPECT_EQ(AddTerms({starred_}, foo_url_), OpResults::kFileMissing);
}

}  // namespace
}  // namespace ash::file_manager
