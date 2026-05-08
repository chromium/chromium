// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/constants.h"
#include "components/sqlite_vfs/multiprocess_test.test-mojom.h"
#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sqlite_vfs {

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;

constexpr base::FilePath::CharType kBaseName[] = FILE_PATH_LITERAL("test_db");

std::optional<SqliteVfsFileSet> CreateAndBindFileSet(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool journal_mode_wal) {
  auto pending_file_set =
      MakePendingFileSet(Client::kTest, directory, base_name,
                         /*single_connection=*/false, journal_mode_wal);
  if (!pending_file_set.has_value()) {
    return std::nullopt;
  }

  return SqliteVfsFileSet::Bind(Client::kTest, *std::move(pending_file_set));
}

sql::DatabaseOptions MakeDatabaseOptionsForFileSet(
    const SqliteVfsFileSet& file_set) {
  return sql::DatabaseOptions()
      .set_exclusive_locking(file_set.is_single_connection())
      .set_wal_mode(file_set.wal_mode())
      .set_vfs_name_discouraged(SqliteSandboxedVfsDelegate::kSqliteVfsName)
      .set_mmap_enabled(false)
      .set_read_only(file_set.read_only());
}

class ReadOnlyConnectionImpl : public sqlite_vfs::mojom::ReadOnlyConnection {
 public:
  explicit ReadOnlyConnectionImpl(sqlite_vfs::PendingFileSet file_set)
      : file_set_(SqliteVfsFileSet::Bind(Client::kTest, std::move(file_set))),
        unregister_runner_(
            SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
                *file_set_)),
        db_(MakeDatabaseOptionsForFileSet(*file_set_),
            sql::Database::Tag("Test")) {
    db_.set_error_callback(base::BindRepeating(
        [](std::vector<int32_t>* errors, int error, sql::Statement* stmt) {
          errors->push_back(error);
        },
        base::Unretained(&reported_errors_)));
    is_open_ = db_.Open(file_set_->GetDbVirtualFilePath());
  }

  bool is_open() const { return is_open_; }

  void Read(ReadCallback callback) override {
    sql::Statement stm(db_.GetUniqueStatement(
        "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
    if (stm.Step()) {
      std::move(callback).Run(base::ok(stm.ColumnString(0)));
    } else if (stm.Succeeded()) {
      std::move(callback).Run(
          base::unexpected(static_cast<int>(sql::SqliteResultCode::kDone)));
    } else {
      std::move(callback).Run(base::unexpected(db_.GetErrorCode()));
    }
  }

  void RunReadSequence(int32_t count,
                       RunReadSequenceCallback callback) override {
    int success_count = 0;
    sql::Statement stm(db_.GetUniqueStatement(
        "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
    for (int i = 0; i < count; ++i) {
      if (stm.Step()) {
        success_count++;
      }
      stm.Reset(true);
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    std::move(callback).Run(success_count);
  }

  void CloseDatabase(CloseDatabaseCallback callback) override {
    db_.Close();
    is_open_ = false;
    std::move(callback).Run(reported_errors_);
  }

 private:
  std::optional<SqliteVfsFileSet> file_set_;
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_;
  std::vector<int32_t> reported_errors_;
  sql::Database db_;
  bool is_open_ = false;
};

class ReadWriteConnectionImpl : public sqlite_vfs::mojom::ReadWriteConnection {
 public:
  explicit ReadWriteConnectionImpl(sqlite_vfs::PendingFileSet file_set)
      : file_set_(SqliteVfsFileSet::Bind(Client::kTest, std::move(file_set))),
        unregister_runner_(
            SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
                *file_set_)),
        db_(MakeDatabaseOptionsForFileSet(*file_set_),
            sql::Database::Tag("Test")) {
    db_.set_error_callback(base::BindRepeating(
        [](std::vector<int32_t>* errors, int error, sql::Statement* stmt) {
          errors->push_back(error);
        },
        base::Unretained(&reported_errors_)));
    is_open_ = db_.Open(file_set_->GetDbVirtualFilePath());
    if (is_open_) {
      std::ignore = db_.Execute("CREATE TABLE IF NOT EXISTS test (val TEXT)");
    }
  }

  bool is_open() const { return is_open_; }

  void Read(ReadCallback callback) override {
    sql::Statement stm(db_.GetUniqueStatement(
        "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
    if (stm.Step()) {
      std::move(callback).Run(base::ok(stm.ColumnString(0)));
    } else if (stm.Succeeded()) {
      std::move(callback).Run(
          base::unexpected(static_cast<int>(sql::SqliteResultCode::kDone)));
    } else {
      std::move(callback).Run(base::unexpected(db_.GetErrorCode()));
    }
  }

  void RunReadSequence(int32_t count,
                       RunReadSequenceCallback callback) override {
    int success_count = 0;
    sql::Statement stm(db_.GetUniqueStatement(
        "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
    for (int i = 0; i < count; ++i) {
      if (stm.Step()) {
        success_count++;
      }
      stm.Reset(true);
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    std::move(callback).Run(success_count);
  }

  void Write(const std::string& value, WriteCallback callback) override {
    bool success = db_.Execute(
        base::StrCat({"INSERT INTO test (val) VALUES ('", value, "')"}));

    std::move(callback).Run(success);
  }

  void RunStressTest(int32_t iterations,
                     RunStressTestCallback callback) override {
    int success_count = 0;
    for (int i = 0; i < iterations; ++i) {
      if (i % 2 == 0) {
        sql::Statement stm(db_.GetUniqueStatement(
            "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
        if (stm.Step()) {
          success_count++;
        }
      } else {
        sql::Statement stm(
            db_.GetUniqueStatement("INSERT INTO test (val) VALUES (?)"));
        stm.BindString(0, base::StrCat({"stress_", base::NumberToString(i)}));
        if (stm.Run()) {
          success_count++;
        }
      }
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    std::move(callback).Run(success_count);
  }

  void Checkpoint(CheckpointCallback callback) override {
    std::move(callback).Run(db_.CheckpointDatabase(/*truncate=*/true));
  }

  void CloseDatabase(CloseDatabaseCallback callback) override {
    db_.Close();
    is_open_ = false;
    std::move(callback).Run(reported_errors_);
  }

 private:
  std::optional<SqliteVfsFileSet> file_set_;
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_;
  std::vector<int32_t> reported_errors_;
  sql::Database db_;
  bool is_open_ = false;
};

class SqliteVfsMultiprocessTestHelperImpl
    : public sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper {
 public:
  explicit SqliteVfsMultiprocessTestHelperImpl(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~SqliteVfsMultiprocessTestHelperImpl() override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void OpenDatabase(sqlite_vfs::PendingFileSet file_set,
                    OpenDatabaseCallback callback) override {
    auto impl = std::make_unique<ReadWriteConnectionImpl>(std::move(file_set));
    if (!impl->is_open()) {
      std::move(callback).Run(mojo::NullRemote());
      return;
    }
    mojo::PendingRemote<sqlite_vfs::mojom::ReadWriteConnection> remote;
    mojo::MakeSelfOwnedReceiver(std::move(impl),
                                remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));
  }

  void OpenDatabaseReadOnly(sqlite_vfs::PendingFileSet file_set,
                            OpenDatabaseReadOnlyCallback callback) override {
    auto impl = std::make_unique<ReadOnlyConnectionImpl>(std::move(file_set));
    if (!impl->is_open()) {
      std::move(callback).Run(mojo::NullRemote());
      return;
    }
    mojo::PendingRemote<sqlite_vfs::mojom::ReadOnlyConnection> remote;
    mojo::MakeSelfOwnedReceiver(std::move(impl),
                                remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

MULTIPROCESS_TEST_MAIN(SqliteVfsChild) {
  base::CommandLine::Init(/*argc=*/0, /*argv=*/nullptr);
  auto& command_line = *base::CommandLine::ForCurrentProcess();

  mojo::core::Init();
  base::Thread ipc_thread("ipc");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          command_line));
  mojo::ScopedMessagePipeHandle pipe =
      invitation.ExtractMessagePipe(/*name=*/0);

  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO,
                                               /*is_main_thread=*/true);

  base::RunLoop run_loop;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SqliteVfsMultiprocessTestHelperImpl>(
          run_loop.QuitClosure()),
      mojo::PendingReceiver<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper>(
          std::move(pipe)));

  run_loop.Run();

  return 0;
}

// A helper to install a `MockCallback` as a database's error callback.
// Expectations may be set on an instance's `Run(int, sql::Statement*)` method.
class ScopedMockErrorCallback
    : public base::MockCallback<sql::Database::ErrorCallback> {
 public:
  explicit ScopedMockErrorCallback(sql::Database& db) : db_(db) {
    db_->set_error_callback(Get());
  }
  ~ScopedMockErrorCallback() { db_->reset_error_callback(); }

 private:
  const raw_ref<sql::Database> db_;
};

class SqliteVfsMultiprocessTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  }

  base::FilePath file_set_directory() const { return temp_dir_.GetPath(); }

  void TerminateLastChild() {
    ASSERT_FALSE(child_processes_.empty());
    base::Process& child_process = child_processes_.back();
    ASSERT_TRUE(child_process.IsValid());
    ASSERT_TRUE(child_process.Terminate(0, true));
    child_processes_.pop_back();
  }

  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> SpawnChild(
      const std::string& name) {
    base::CommandLine child_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();

    mojo::PlatformChannel channel;
    mojo::OutgoingInvitation invitation;
    mojo::ScopedMessagePipeHandle pipe =
        invitation.AttachMessagePipe(/*name=*/0);

    base::LaunchOptions launch_options;
    channel.PrepareToPassRemoteEndpoint(&launch_options, &child_command_line);

    base::Process process = base::SpawnMultiProcessTestChild(
        name, child_command_line, launch_options);
    EXPECT_TRUE(process.IsValid());

    mojo::OutgoingInvitation::Send(std::move(invitation), process.Handle(),
                                   channel.TakeLocalEndpoint());

    mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> remote;
    remote.Bind(
        mojo::PendingRemote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper>(
            std::move(pipe), 0));

    child_processes_.push_back(std::move(process));

    return remote;
  }

  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> OpenDatabase(
      mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper>& child,
      sqlite_vfs::PendingFileSet file_set) {
    base::test::TestFuture<
        mojo::PendingRemote<sqlite_vfs::mojom::ReadWriteConnection>>
        future;
    child->OpenDatabase(std::move(file_set), future.GetCallback());
    auto remote = future.Take();
    EXPECT_TRUE(remote.is_valid());
    return mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection>(
        std::move(remote));
  }

  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> OpenDatabaseReadOnly(
      mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper>& child,
      sqlite_vfs::PendingFileSet file_set) {
    base::test::TestFuture<
        mojo::PendingRemote<sqlite_vfs::mojom::ReadOnlyConnection>>
        future;
    child->OpenDatabaseReadOnly(std::move(file_set), future.GetCallback());
    auto remote = future.Take();
    EXPECT_TRUE(remote.is_valid());
    return mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection>(
        std::move(remote));
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  std::vector<base::Process> child_processes_;
};

class SqliteVfsMultiprocessTest
    : public SqliteVfsMultiprocessTestBase,
      public ::testing::WithParamInterface<bool /* journal_mode_wal */> {
 protected:
  static bool journal_mode_wal() { return GetParam(); }
};

TEST_P(SqliteVfsMultiprocessTest, ReaderBlocksWriterInRollback) {
  if (journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for rollback journal mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/false));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
    ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/true));
  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
      OpenDatabase(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());

  // Read to acquire shared lock.
  {
    sql::Statement stm(db.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(stm.Step());
    ASSERT_EQ(stm.ColumnString(0), "initial");

    // Call Write on child. It should fail because we hold SHARED lock.
    base::test::TestFuture<bool> future;
    connection->Write("child_value", future.GetCallback());
    bool success = future.Get();

    EXPECT_FALSE(success);
  }  // stm is destroyed here!

  // Commit transaction.
  ASSERT_TRUE(transaction.Commit());

  // Close DB in parent to release all locks!
  db.Close();

  // Call Write on child again. It should succeed now.
  {
    base::test::TestFuture<bool> future;
    connection->Write("child_value", future.GetCallback());
    bool success = future.Get();

    EXPECT_TRUE(success);
  }
}

TEST_P(SqliteVfsMultiprocessTest, WriterBlocksReaderInRollback) {
  if (journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for rollback journal mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/false));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/false));
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection =
      OpenDatabaseReadOnly(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Re-open DB in parent.
  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('parent_value')"));

  // Call Read on child. It should succeed but return nullopt (or not
  // 'parent_value').
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(),
                ErrorIs(static_cast<int>(sql::SqliteResultCode::kDone)));
  }

  // Commit transaction.
  ASSERT_TRUE(transaction.Commit());

  // Close DB in parent to release all locks!
  db.Close();

  // Call Read on child again. It should see the new value now.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("parent_value")));
  }
}

TEST_P(SqliteVfsMultiprocessTest, ReaderDoesNotBlockWriterInWal) {
  if (!journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for WAL mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/true));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
    ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/true));
  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
      OpenDatabase(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Re-open DB in parent.
  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());

  // Read to acquire shared lock (snapshot).
  {
    sql::Statement stm(db.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(stm.Step());
    ASSERT_EQ(stm.ColumnString(0), "initial");
  }

  // Call Write on child. It should SUCCEED in WAL mode!
  {
    base::test::TestFuture<bool> future;
    connection->Write("child_value", future.GetCallback());
    bool success = future.Get();

    EXPECT_TRUE(success);
  }

  // Read again (in same transaction) and should STILL see "initial"!
  {
    sql::Statement stm2(db.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(stm2.Step());
    ASSERT_EQ(stm2.ColumnString(0), "initial");
  }

  // Commit transaction.
  ASSERT_TRUE(transaction.Commit());

  // Close DB in parent to release all locks!
  db.Close();

  // Force checkpoint from child now that parent has closed its connection!
  {
    base::test::TestFuture<bool> future;
    connection->Checkpoint(future.GetCallback());
    bool success = future.Get();
    ASSERT_TRUE(success);
  }

  base::test::TestFuture<std::vector<int32_t>> future;
  connection->CloseDatabase(base::BindOnce(
      [](base::OnceCallback<void(std::vector<int32_t>)> cb,
         const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
      future.GetCallback()));
  EXPECT_THAT(future.Take(), IsEmpty());

  // Re-open DB in parent to see if it picks up the change!
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  sql::Statement stm3(
      db.GetUniqueStatement("SELECT val FROM test ORDER BY rowid DESC"));
  ASSERT_TRUE(stm3.Step());
  EXPECT_THAT(stm3.ColumnString(0), Eq(std::string("child_value")));
}

TEST_P(SqliteVfsMultiprocessTest, WalReadMarkNecessity) {
  if (!journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for WAL mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/true));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  // 1. Parent creates DB and inserts initial data.
  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
    ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));
  }

  // Spawn child (Reader).
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass READ-ONLY file set to child.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/false));
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection =
      OpenDatabaseReadOnly(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Child should see 'initial'!
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("initial")));
  }

  // Parent (Writer) opens DB again to write new data!
  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  // Write new data!
  ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('new_data')"));

  // Child tries to read WITHOUT UpdateReadMark!
  // It should see 'new_data' because the writer likely updated a read-mark
  // during its transaction, which the read-only reader can use.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("new_data")));
  }

  // Cleanup.
  base::test::TestFuture<std::vector<int32_t>> close_future;
  connection->CloseDatabase(base::BindOnce(
      [](base::OnceCallback<void(std::vector<int32_t>)> cb,
         const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
      close_future.GetCallback()));
  EXPECT_THAT(close_future.Take(), IsEmpty());
}

TEST_P(SqliteVfsMultiprocessTest, WalSourceTest) {
  if (!journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for WAL mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/true));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
    ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/true));
  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
      OpenDatabase(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Parent writes more data (goes to WAL).
  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));
    ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('wal_value')"));
  }

  // Child reads. It should see 'wal_value' (reading from WAL).
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("wal_value")));
  }

  // Parent checkpoints.
  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));
    ASSERT_TRUE(db.CheckpointDatabase(/*truncate=*/true));
  }

  // Child reads again. It should still see 'wal_value' (now reading from main
  // DB).
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("wal_value")));
  }
}

TEST_P(SqliteVfsMultiprocessTest, ParallelStressTest) {
  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           journal_mode_wal()));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));
  ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
  ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&error_mock));

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/true));
  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
      OpenDatabase(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Run stress test on child in parallel!
  base::test::TestFuture<int> child_future;
  connection->RunStressTest(100, child_future.GetCallback());

  // Run stress test on parent too!
  EXPECT_CALL(error_mock, Run(static_cast<int>(sql::SqliteErrorCode::kBusy), _))
      .Times(AnyNumber());
  int parent_success_count = 0;
  for (int i = 0; i < 100; ++i) {
    if (i % 2 == 0) {
      sql::Statement stm(db.GetUniqueStatement(
          "SELECT val FROM test ORDER BY rowid DESC LIMIT 1"));
      if (stm.Step()) {
        parent_success_count++;
      }
    } else {
      sql::Statement stm(
          db.GetUniqueStatement("INSERT INTO test (val) VALUES (?)"));
      stm.BindString(0, base::StrCat({"parent_", base::NumberToString(i)}));
      if (stm.Run()) {
        parent_success_count++;
      }
    }
    base::PlatformThread::Sleep(base::Milliseconds(1));
  }

  int child_success_count = child_future.Get();

  EXPECT_GT(parent_success_count, 0);
  EXPECT_GT(child_success_count, 0);
}

TEST_P(SqliteVfsMultiprocessTest, WalDynamicGrowth) {
  if (!journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for WAL mode";
  }

  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           /*journal_mode_wal=*/true));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  {
    sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                     sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
    ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db.Execute("CREATE TABLE test (val BLOB)"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass READ-ONLY file set to child and open database.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/false));
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection =
      OpenDatabaseReadOnly(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Reader performs a read BEFORE the writer does its work.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(),
                ErrorIs(static_cast<int>(sql::SqliteResultCode::kDone)));
  }

  // Main process writes massive transactions to grow the WAL-index (-shm file).
  sql::Database db(
      MakeDatabaseOptionsForFileSet(file_set)
          // Disable automatic checkpointing to grow the write-ahead log.
          .set_wal_commit_callback(base::DoNothing())
          // Disable sync so the test isn't slower than it needs to be.
          .set_no_sync_on_wal_mode(true),
      sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  auto wal_index_size =
      file_set.GetSandboxedDbFile()->GetWalIndexFile().GetLength();

  // Insert a whole lot of data. The write-ahead log will be big after this, and
  // the WAL-index will have grown a few times.
  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  auto blob =
      base::MakeRefCounted<base::RefCountedString>(std::string(4096, 'x'));
  sql::Statement insert_stmt(
      db.GetUniqueStatement("INSERT INTO test (val) VALUES (?)"));
  for (int i = 0; i < 25000; ++i) {
    insert_stmt.BindBlob(0, blob);
    ASSERT_TRUE(insert_stmt.Run());
    insert_stmt.Reset(true);
  }
  ASSERT_TRUE(transaction.Commit());

  // Verify that the WAL-index has grown.
  EXPECT_GT(file_set.GetSandboxedDbFile()->GetWalIndexFile().GetLength(),
            wal_index_size);

  // A read-only connection makes a read, causing it to check the WAL-index and
  // WAL.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_OK(future.Take());
  }

  wal_index_size = file_set.GetSandboxedDbFile()->GetWalIndexFile().GetLength();

  // Grow the WAL even more.
  {
    sql::Transaction transaction2(&db);
    ASSERT_TRUE(transaction2.Begin());
    for (int i = 0; i < 10000; ++i) {
      insert_stmt.BindBlob(0, blob);
      ASSERT_TRUE(insert_stmt.Run());
      insert_stmt.Reset(true);
    }
    ASSERT_TRUE(transaction2.Commit());
  }

  // Verify that the WAL-index has grown more.
  EXPECT_GT(file_set.GetSandboxedDbFile()->GetWalIndexFile().GetLength(),
            wal_index_size);

  // A read-only connection makes another read, causing it to find new data in
  // the WAL-index.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_OK(future.Take());
  }

  // Cleanup.
  base::test::TestFuture<std::vector<int32_t>> close_future;
  connection->CloseDatabase(base::BindOnce(
      [](base::OnceCallback<void(std::vector<int32_t>)> cb,
         const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
      close_future.GetCallback()));
  EXPECT_THAT(close_future.Take(), IsEmpty());
}

TEST_P(SqliteVfsMultiprocessTest, ReadOnlyFirstConnection) {
  if (!journal_mode_wal()) {
    GTEST_SKIP() << "This test is only for WAL mode";
  }

  // Phase 1: Child process creates shared WAL database and inserts data.
  {
    ASSERT_OK_AND_ASSIGN(
        auto file_set,
        CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                             /*journal_mode_wal=*/true));
    auto unregister_runner =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            file_set);

    mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
        SpawnChild("SqliteVfsChild");

    ASSERT_OK_AND_ASSIGN(auto pending_file_set_child,
                         ShareConnection(file_set_directory(),
                                         base::FilePath(kBaseName), file_set,
                                         /*read_write=*/true));
    mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
        OpenDatabase(child, std::move(pending_file_set_child));
    ASSERT_TRUE(connection.is_bound());

    // Write data without checkpointing.
    {
      base::test::TestFuture<bool> future;
      connection->Write("hello", future.GetCallback());
      ASSERT_TRUE(future.Get());
    }

    // Forcefully terminate the child to leave WAL index and WAL files behind.
    TerminateLastChild();
  }

  // Phase 2: Create a new connection to the same files.
  ASSERT_OK_AND_ASSIGN(auto pending_rw,
                       MakePendingFileSet(Client::kTest, file_set_directory(),
                                          base::FilePath(kBaseName),
                                          /*single_connection=*/false,
                                          /*journal_mode_wal=*/true));

  ASSERT_OK_AND_ASSIGN(
      auto file_set_rw_parent,
      SqliteVfsFileSet::Bind(Client::kTest, std::move(pending_rw)));

  // Share a read-only connection.
  ASSERT_OK_AND_ASSIGN(
      auto pending_ro,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName),
                      file_set_rw_parent, /*read_write=*/false));

  ASSERT_OK_AND_ASSIGN(
      auto file_set_ro,
      SqliteVfsFileSet::Bind(Client::kTest, std::move(pending_ro)));

  // Register read-only files with VFS.
  SqliteSandboxedVfsDelegate::UnregisterRunner ro_unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set_ro);

  sql::Database db_ro(MakeDatabaseOptionsForFileSet(file_set_ro),
                      sql::Database::Tag("Test"));
  // Open read-only connection.
  ASSERT_TRUE(db_ro.Open(file_set_ro.GetDbVirtualFilePath()));

  // Verify that data can be read!
  {
    sql::Statement s(db_ro.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "hello");
  }

  // Phase 3: Open the read-write connection in SQLite.
  SqliteSandboxedVfsDelegate::UnregisterRunner rw_unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set_rw_parent);

  sql::Database db_rw(MakeDatabaseOptionsForFileSet(file_set_rw_parent),
                      sql::Database::Tag("Test"));
  ASSERT_TRUE(db_rw.Open(file_set_rw_parent.GetDbVirtualFilePath()));

  // Verify both can read.
  {
    sql::Statement s(db_ro.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "hello");
  }
  {
    sql::Statement s(db_rw.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "hello");
  }

  // Phase 4: Checkpoint the database.
  EXPECT_TRUE(db_rw.CheckpointDatabase());

  // Verify both can read.
  {
    sql::Statement s(db_ro.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "hello");
  }
  {
    sql::Statement s(db_rw.GetUniqueStatement("SELECT val FROM test"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "hello");
  }

  db_ro.Close();
  db_rw.Close();
}

TEST_P(SqliteVfsMultiprocessTest, AbandonReadOnlyShared) {
  ASSERT_OK_AND_ASSIGN(
      auto file_set,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           journal_mode_wal()));
  auto unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set);

  sql::Database db(MakeDatabaseOptionsForFileSet(file_set),
                   sql::Database::Tag("Test"));
  ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db);
  ASSERT_TRUE(db.Open(file_set.GetDbVirtualFilePath()));

  // Create table.
  ASSERT_TRUE(db.Execute("CREATE TABLE test (val TEXT)"));
  ASSERT_TRUE(db.Execute("INSERT INTO test (val) VALUES ('initial')"));

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass READ-ONLY file set to child.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/false));
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection =
      OpenDatabaseReadOnly(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Child should see 'initial'!
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs("initial"));
  }

  // Parent calls Abandon on the file set.
  file_set.Abandon();

  // Child tries to read again. It should fail with SQLITE_IOERR_LOCK.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection->Read(future.GetCallback());
    EXPECT_THAT(future.Take(),
                ErrorIs(static_cast<int>(sql::SqliteErrorCode::kIoLock)));
  }

  // Parent closes database.
  db.Close();

  // Cleanup.
  base::test::TestFuture<std::vector<int32_t>> close_future;
  connection->CloseDatabase(base::BindOnce(
      [](base::OnceCallback<void(std::vector<int32_t>)> cb,
         const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
      close_future.GetCallback()));
  EXPECT_THAT(close_future.Take(),
              ElementsAre(static_cast<int>(sql::SqliteErrorCode::kIoLock)));
}

TEST_P(SqliteVfsMultiprocessTest, AbandonAndReconnect) {
  ASSERT_OK_AND_ASSIGN(
      auto file_set_1,
      CreateAndBindFileSet(file_set_directory(), base::FilePath(kBaseName),
                           journal_mode_wal()));
  {
    auto unregister_runner_1 =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            file_set_1);

    sql::Database db_1(MakeDatabaseOptionsForFileSet(file_set_1),
                       sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db_1);
    ASSERT_TRUE(db_1.Open(file_set_1.GetDbVirtualFilePath()));

    // Create table.
    ASSERT_TRUE(db_1.Execute("CREATE TABLE test (val TEXT)"));
    ASSERT_TRUE(db_1.Execute("INSERT INTO test (val) VALUES ('initial')"));
  }

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass read-only file set to child.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set_1,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName),
                      file_set_1, /*read_write=*/false));
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection_1 =
      OpenDatabaseReadOnly(child, std::move(pending_file_set_1));
  ASSERT_TRUE(connection_1.is_bound());

  // Child should see 'initial'.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection_1->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs("initial"));
  }

  // Parent Abandons the file set.
  file_set_1.Abandon();

  // Parent creates a new connection to the same physical database files.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set_2,
      MakePendingFileSet(Client::kTest, file_set_directory(),
                         base::FilePath(kBaseName),
                         /*single_connection=*/false, journal_mode_wal()));
  ASSERT_OK_AND_ASSIGN(
      auto file_set_2,
      SqliteVfsFileSet::Bind(Client::kTest, std::move(pending_file_set_2)));
  auto unregister_runner_2 =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set_2);

  {
    sql::Database db_2(MakeDatabaseOptionsForFileSet(file_set_2),
                       sql::Database::Tag("Test"));
    ::testing::StrictMock<ScopedMockErrorCallback> error_mock(db_2);
    ASSERT_TRUE(db_2.Open(file_set_2.GetDbVirtualFilePath()));
    ASSERT_TRUE(db_2.Execute("INSERT INTO test (val) VALUES ('new_value')"));
  }

  // Parent shares this new connection with the child.
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set_2_ro,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName),
                      file_set_2, /*read_write=*/false));

  // Child succeeds in opening a second connection because the first has been
  // abandoned.
  base::test::TestFuture<
      mojo::PendingRemote<sqlite_vfs::mojom::ReadOnlyConnection>>
      future_2;
  child->OpenDatabaseReadOnly(std::move(pending_file_set_2_ro),
                              future_2.GetCallback());
  auto remote_2 = future_2.Take();
  ASSERT_TRUE(remote_2.is_valid());
  mojo::Remote<sqlite_vfs::mojom::ReadOnlyConnection> connection_2(
      std::move(remote_2));

  // Verify that child can use the new connection to read the new value.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection_2->Read(future.GetCallback());
    EXPECT_THAT(future.Take(), ValueIs(std::string("new_value")));
  }

  // Verify that child gets a kIoLock error on connection_1.
  {
    base::test::TestFuture<base::expected<std::string, int32_t>> future;
    connection_1->Read(future.GetCallback());
    EXPECT_THAT(future.Take(),
                ErrorIs(static_cast<int>(sql::SqliteErrorCode::kIoLock)));
  }

  // Cleanup.
  {
    base::test::TestFuture<std::vector<int32_t>> close_future;
    connection_1->CloseDatabase(base::BindOnce(
        [](base::OnceCallback<void(std::vector<int32_t>)> cb,
           const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
        close_future.GetCallback()));
    EXPECT_THAT(close_future.Take(),
                ElementsAre(static_cast<int>(sql::SqliteErrorCode::kIoLock)));
  }
  {
    base::test::TestFuture<std::vector<int32_t>> close_future;
    connection_2->CloseDatabase(base::BindOnce(
        [](base::OnceCallback<void(std::vector<int32_t>)> cb,
           const std::vector<int32_t>& errors) { std::move(cb).Run(errors); },
        close_future.GetCallback()));
    EXPECT_THAT(close_future.Take(), IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SqliteVfsMultiprocessTest,
                         Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "Wal" : "Rollback";
                         });

class VfsTransitionMultiprocessTest
    : public SqliteVfsMultiprocessTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  static bool single_connection() { return std::get<0>(GetParam()); }
  static bool journal_mode_wal() { return std::get<1>(GetParam()); }
};

TEST_P(VfsTransitionMultiprocessTest, TransitionAfterCrash) {
  const bool single_conn = single_connection();
  const bool wal = journal_mode_wal();

  const base::FilePath base_name(kBaseName);
  const Client client = Client::kTest;

  // Step 1: Create file set with parameterized options.
  ASSERT_OK_AND_ASSIGN(auto pending_file_set,
                       MakePendingFileSet(client, file_set_directory(),
                                          base_name, single_conn, wal));

  // Spawn child.
  mojo::Remote<sqlite_vfs::mojom::SqliteVfsMultiprocessTestHelper> child =
      SpawnChild("SqliteVfsChild");

  // Pass file set to child.
  mojo::Remote<sqlite_vfs::mojom::ReadWriteConnection> connection =
      OpenDatabase(child, std::move(pending_file_set));
  ASSERT_TRUE(connection.is_bound());

  // Child writes to database.
  {
    base::test::TestFuture<bool> future;
    connection->Write("child_value", future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

  // Terminate child process abruptly.
  TerminateLastChild();

  // Step 3: Make new pending file set with opposite journal mode in parent.
  ASSERT_OK_AND_ASSIGN(auto pending_file_set_2,
                       MakePendingFileSet(client, file_set_directory(),
                                          base_name, single_conn, !wal));

  ASSERT_OK_AND_ASSIGN(
      auto file_set_2,
      SqliteVfsFileSet::Bind(client, std::move(pending_file_set_2)));

  // Register files in parent.
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_2 =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          file_set_2);

  // Verify database can be opened.
  sql::Database db_2(MakeDatabaseOptionsForFileSet(file_set_2), "Test");

  EXPECT_TRUE(db_2.Open(file_set_2.GetDbVirtualFilePath()));

  // Confirm that the database is using the expected journal mode.
  {
    sql::Statement s(db_2.GetUniqueStatement("PRAGMA journal_mode"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), !wal ? "wal" : "truncate");
  }

  // Execute a statement and confirm once again.
  ASSERT_TRUE(db_2.Execute("INSERT INTO test (val) VALUES ('later')"));
  {
    sql::Statement s(db_2.GetUniqueStatement("PRAGMA journal_mode"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), !wal ? "wal" : "truncate");
  }

  // Confirm that the unused journal file is empty.
  auto* extension = wal ? kWalJournalFileExtension : kJournalFileExtension;
  EXPECT_EQ(base::GetFileSize(file_set_directory()
                                  .Append(base::FilePath(kBaseName))
                                  .AddExtension(extension)),
            0LL);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VfsTransitionMultiprocessTest,
    Combine(Bool(), Bool()),
    [](const ::testing::TestParamInfo<VfsTransitionMultiprocessTest::ParamType>&
           info) {
      const bool single_connection = std::get<0>(info.param);
      const bool journal_mode_wal = std::get<1>(info.param);
      return std::string(single_connection ? "SingleConnection"
                                           : "MultiConnection") +
             "_" + (journal_mode_wal ? "Wal" : "Rollback");
    });

}  // namespace sqlite_vfs
