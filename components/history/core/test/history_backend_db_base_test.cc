// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_backend_db_base_test.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/test/test_helpers.h"
#include "url/gurl.h"

namespace history {

// Delegate class for when we create a backend without a HistoryService.
class BackendDelegate : public HistoryBackend::Delegate {
 public:
  explicit BackendDelegate(HistoryBackendDBBaseTest* history_test)
      : history_test_(history_test) {}

  // HistoryBackend::Delegate implementation.
  bool CanAddURL(const GURL& url) const override { return true; }
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {
    history_test_->last_profile_error_ = init_status;
  }
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {
    // Save the in-memory backend to the history test object, this happens
    // synchronously, so we don't have to do anything fancy.
    history_test_->in_mem_backend_.swap(backend);
  }
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(VisitedURLInfo visited_url_info) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyDeletions(DeletionInfo deletion_info) override {}
  void NotifyVisitedLinksAdded(const HistoryAddPageArgs& args) override {}
  void NotifyVisitedLinksDeleted(
      const std::vector<DeletedVisitedLink>& links) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

 private:
  raw_ptr<HistoryBackendDBBaseTest> history_test_;
};

HistoryBackendDBBaseTest::HistoryBackendDBBaseTest()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
      db_(nullptr),
      last_profile_error_(sql::INIT_OK) {}

HistoryBackendDBBaseTest::~HistoryBackendDBBaseTest() {
}

void HistoryBackendDBBaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  history_dir_ = temp_dir_.GetPath().AppendASCII("HistoryBackendDBBaseTest");
  ASSERT_TRUE(base::CreateDirectory(history_dir_));
}

void HistoryBackendDBBaseTest::TearDown() {
  DeleteBackend();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  base::RunLoop().RunUntilIdle();
}

bool HistoryBackendDBBaseTest::CreateBackendAndDatabase() {
  backend_ = base::MakeRefCounted<HistoryBackend>(
      std::make_unique<BackendDelegate>(this), nullptr,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  backend_->Init(false,
                 TestHistoryDatabaseParamsForPath(history_dir_));
  db_ = backend_->db_.get();
  return in_mem_backend_ != nullptr;
}

void HistoryBackendDBBaseTest::CreateDBVersion(int version) {
  base::FilePath data_path;
  ASSERT_TRUE(GetTestDataHistoryDir(&data_path));
  data_path =
      data_path.AppendASCII(base::StringPrintf("history.%d.sql", version));
  ASSERT_NO_FATAL_FAILURE(
      ExecuteSQLScript(data_path, history_dir_.Append(kHistoryFilename)));
}

int HistoryBackendDBBaseTest::GetDatabaseVersion() const {
  sql::Database db(sql::test::kTestTag);
  CHECK(db.Open(history_dir_.Append(kHistoryFilename)));
  return sql::InitializedMetaTable(db).GetVersionNumber();
}

bool HistoryBackendDBBaseTest::SetDatabaseVersion(int version) const {
  sql::Database db(sql::test::kTestTag);
  CHECK(db.Open(history_dir_.Append(kHistoryFilename)));
  return sql::InitializedMetaTable(db).SetVersionNumber(version);
}

void HistoryBackendDBBaseTest::DeleteBackend() {
  if (backend_) {
    // The backend is ref-counted and won't be deleted right away if a database
    // error queued a `KillHistoryDatabase` task (which holds a reference on the
    // backend). Thus, releasing the backend pointer isn't enough here, we need
    // to explicitly wait for the object to be deleted.
    base::RunLoop loop;
    backend_->SetOnBackendDestroyTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(), loop.QuitClosure());

    backend_->Closing();
    db_ = nullptr;
    backend_ = nullptr;

    loop.Run();
  }
}

bool HistoryBackendDBBaseTest::AddDownload(uint32_t id,
                                           const std::string& guid,
                                           DownloadState state,
                                           base::Time time) {
  DownloadRow download;
  download.current_path = base::FilePath(FILE_PATH_LITERAL("current-path"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("target-path"));
  download.url_chain.push_back(GURL("foo-url"));
  download.referrer_url = GURL("http://referrer.example.com/");
  download.site_url = GURL("http://site-url.example.com");
  download.embedder_download_data = "embedder_download_data";
  download.tab_url = GURL("http://tab-url.example.com/");
  download.tab_referrer_url = GURL("http://tab-referrer-url.example.com/");
  download.http_method = std::string();
  download.mime_type = "application/vnd.oasis.opendocument.text";
  download.original_mime_type = "application/octet-stream";
  download.start_time = time;
  download.end_time = time;
  download.received_bytes = 0;
  download.total_bytes = 512;
  download.state = state;
  download.danger_type = DownloadDangerType::NOT_DANGEROUS;
  download.interrupt_reason = kTestDownloadInterruptReasonNone;
  download.id = id;
  download.guid = guid;
  download.opened = false;
  download.last_access_time = time;
  download.transient = true;
  download.by_ext_id = "by_ext_id";
  download.by_ext_name = "by_ext_name";
  download.by_web_app_id = "by_web_app_id";
  return db_->CreateDownload(download);
}

}  // namespace history
