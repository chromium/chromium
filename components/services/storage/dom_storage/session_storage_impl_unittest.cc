// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_impl.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/services/storage/dom_storage/storage_area_test_util.h"
#include "components/services/storage/dom_storage/testing_legacy_session_storage_database.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> StringViewToUint8Vector(std::string_view s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> String16ToUint8Vector(const std::u16string& s) {
  auto bytes = base::as_bytes(base::make_span(s));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

static const char kSessionStorageDirectory[] = "Session Storage";

class SessionStorageImplTest : public testing::Test {
 public:
  SessionStorageImplTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  SessionStorageImplTest(const SessionStorageImplTest&) = delete;
  SessionStorageImplTest& operator=(const SessionStorageImplTest&) = delete;

  ~SessionStorageImplTest() override {
    EXPECT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &SessionStorageImplTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    if (session_storage_)
      ShutDownSessionStorage();
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  void SetBackingMode(SessionStorageImpl::BackingMode backing_mode) {
    DCHECK(!session_storage_);
    backing_mode_ = backing_mode;
  }

  SessionStorageImpl* session_storage_impl() {
    if (!session_storage_) {
      remote_session_storage_.reset();
      session_storage_ = std::make_unique<SessionStorageImpl>(
          temp_path(), blocking_task_runner_,
          base::SequencedTaskRunner::GetCurrentDefault(), backing_mode_,
          kSessionStorageDirectory,
          remote_session_storage_.BindNewPipeAndPassReceiver());
    }
    return session_storage_.get();
  }

  mojom::SessionStorageControl* session_storage() {
    session_storage_impl();
    return remote_session_storage_.get();
  }

  void ShutDownSessionStorage() {
    remote_session_storage_.FlushForTesting();

    base::RunLoop loop;
    session_storage_->ShutDown(loop.QuitClosure());
    loop.Run();
    session_storage_.reset();
  }

  void DoTestPut(const std::string& namespace_id,
                 const blink::StorageKey& storage_key,
                 std::string_view key,
                 std::string_view value,
                 const std::string& source) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(storage_key, namespace_id,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector(key),
                              StringViewToUint8Vector(value), std::nullopt,
                              source));
    session_storage()->DeleteNamespace(namespace_id, true);
  }

  std::optional<std::vector<uint8_t>> DoTestGet(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key,
      std::string_view key) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(storage_key, namespace_id,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());

    // Use the GetAll interface because Gets are being removed.
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area.get(), &data));
    session_storage()->DeleteNamespace(namespace_id, true);

    std::vector<uint8_t> key_as_bytes = StringViewToUint8Vector(key);
    for (const auto& key_value : data) {
      if (key_value->key == key_as_bytes) {
        return key_value->value;
      }
    }
    return std::nullopt;
  }

 protected:
  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FlushMojo() { remote_session_storage_.FlushForTesting(); }

  bool bad_message_called_ = false;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  SessionStorageImpl::BackingMode backing_mode_ =
      SessionStorageImpl::BackingMode::kRestoreDiskState;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})};
  std::unique_ptr<SessionStorageImpl> session_storage_;
  mojo::Remote<mojom::SessionStorageControl> remote_session_storage_;
};

TEST_F(SessionStorageImplTest, MigrationV0ToV1) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  std::u16string key = u"key";
  std::u16string value = u"value";
  std::u16string key2 = u"key2";
  key2.push_back(0xd83d);
  key2.push_back(0xde00);

  base::FilePath old_db_path =
      temp_path().AppendASCII(kSessionStorageDirectory);
  {
    auto db = base::MakeRefCounted<TestingLegacySessionStorageDatabase>(
        old_db_path, base::SingleThreadTaskRunner::GetCurrentDefault().get());
    LegacyDomStorageValuesMap data;
    data[key] = value;
    data[key2] = value;
    EXPECT_TRUE(
        db->CommitAreaChanges(namespace_id1, storage_key1, false, data));
    EXPECT_TRUE(db->CloneNamespace(namespace_id1, namespace_id2));
  }
  EXPECT_TRUE(base::PathExists(old_db_path));

  // The first call to session_storage() here constructs it.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->CreateNamespace(namespace_id2);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n2_o1;
  mojo::Remote<blink::mojom::StorageArea> area_n2_o2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(storage_key2, namespace_id2,
                                     area_n2_o2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2_o1.get(), &data));
  // There should have been a migration to get rid of the "map-0-" refcount
  // field.
  EXPECT_EQ(2ul, data.size());
  std::vector<uint8_t> key_as_vector =
      StdStringToUint8Vector(base::UTF16ToUTF8(key));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
}

TEST_F(SessionStorageImplTest, StartupShutdownSave) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Verify data is there.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace, shut down Session Storage, and do not persist the
  // data.
  session_storage()->DeleteNamespace(namespace_id1, false);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and the namespace should be empty.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should not be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, CloneBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();

  // Do the browser-side clone afterwards.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, Cloning) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Internally triggered clone before the put. The clone doesn't actually count
  // until a clone comes from the namespace.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();
  area_n1.reset();
  ss_namespace1.reset();

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again. This tests the case where our cloning
  // works even though the namespace is deleted (but persisted on disk).
  session_storage()->DeleteNamespace(namespace_id1, true);

  // The data from before should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Put some data in namespace 2.
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringViewToUint8Vector("key2"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(2ul, data.size());

  // Re-open namespace 1, check that we don't have the extra data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // We should only have the first value.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, ImmediateCloning) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Immediate clone.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure empty.
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(0ul, data.size());
  }

  // Delete that namespace, copy again after a put.
  session_storage()->DeleteNamespace(namespace_id2, false);
  FlushMojo();

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));

  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure populated
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(1ul, data.size());
  }

  session_storage()->DeleteNamespace(namespace_id2, false);
  FlushMojo();

  // Verify that cloning from the namespace object will result in a bad message.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // This should cause a bad message.
  ss_namespace1->Clone(namespace_id2);
  ss_namespace1.FlushForTesting();

  EXPECT_TRUE(bad_message_called_);
}

TEST_F(SessionStorageImplTest, Scavenging) {
  // Create our namespace, shut down Session Storage, and leave that namespace
  // on disk; then verify that it is scavenged if we re-initialize Session
  // Storage without calling CreateNamespace.

  // Create, verify we have no data.
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  // This scavenge call should NOT delete the namespace, as we just created it.
  {
    base::RunLoop loop;
    // Cause the connection to start loading, so we start scavenging mid-load.
    session_storage()->Flush();
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area_n1.reset();

  // This scavenge call should NOT delete the namespace, as we never called
  // delete.
  session_storage()->ScavengeUnusedNamespaces(base::DoNothing());

  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // This scavenge call should NOT delete the namespace, as we explicitly
  // persisted the namespace.
  {
    base::RunLoop loop;
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }

  ShutDownSessionStorage();

  // Re-initialize Session Storage, load the persisted namespace, and verify we
  // still have data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Shutting down Session Storage without an explicit DeleteNamespace
  // should leave the data on disk.
  ShutDownSessionStorage();

  // Re-initialize Session Storage. Scavenge should now remove the namespace as
  // there has been no call to CreateNamespace. Check that the data is
  // empty.
  {
    base::RunLoop loop;
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InvalidVersionOnDisk) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, storage_key, "key", "value", "source");
  std::optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();
  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path =
        temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "version", "argh").ok());
  }

  opt_value = DoTestGet(namespace_id, storage_key, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, storage_key, "key", "value", "source");

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, CorruptionOnDisk) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, storage_key, "key", "value", "source");
  std::optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();
  // Also flush Task Scheduler tasks to make sure the leveldb is fully closed.
  RunUntilIdle();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path =
      temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name);
  }
  opt_value = DoTestGet(namespace_id, storage_key, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, storage_key, "key", "value", "source");

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, storage_key, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, RecreateOnCommitFailure) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://asf.com");
  blink::StorageKey storage_key3 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");

  std::optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area_o1;
  mojo::Remote<blink::mojom::StorageArea> area_o2;
  mojo::Remote<blink::mojom::StorageArea> area_o3;
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindNamespace(namespace_id,
                                   ss_namespace.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(storage_key2, namespace_id,
                                     area_o2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(storage_key3, namespace_id,
                                     area_o3.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  open_loop->Run();

  // Ensure that the first opened database always fails to write data.
  session_storage_impl()->GetDatabaseForTesting().PostTaskWithThisObject(
      base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Also prepare for another database connection, next time providing a
  // functioning database.
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first storage_key. This put operation should result in
  // a pending commit that will get cancelled when the database connection is
  // closed.
  auto value = StringViewToUint8Vector("avalue");
  area_o3->Put(StringViewToUint8Vector("w3key"), value, std::nullopt, "source",
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  while (area_o1.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    std::vector<uint8_t> old_value = value;
    value[0]++;
    area_o1->Put(StringViewToUint8Vector("key"), value, std::nullopt, "source",
                 base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area_o1.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);
  }
  area_o1.reset();

  // Wait for a new database to be opened, which should happen after the first
  // database is destroyed due to failures.
  open_loop->Run();
  EXPECT_EQ(1u, num_databases_destroyed);
  EXPECT_EQ(2u, num_database_open_requests);

  // The connection to the second area should have closed as well.
  area_o2.FlushForTesting();
  ss_namespace.FlushForTesting();
  EXPECT_FALSE(area_o2.is_connected());
  EXPECT_FALSE(ss_namespace.is_connected());

  // Reconnect area_o1 to the new database, and try to read a value.
  ss_namespace.reset();
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  base::RunLoop delete_loop;
  bool success = true;
  test::MockLevelDBObserver observer4;
  area_o1->AddObserver(observer4.Bind());
  area_o1->Delete(StringViewToUint8Vector("key"), std::nullopt, "source",
                  base::BindLambdaForTesting([&](bool success_in) {
                    success = success_in;
                    delete_loop.Quit();
                  }));

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  area_o1.reset();
  session_storage()->DeleteNamespace(namespace_id, true);

  {
    // Committing data should now work.
    DoTestPut(namespace_id, storage_key1, "key", "value", "source");
    std::optional<std::vector<uint8_t>> opt_value =
        DoTestGet(namespace_id, storage_key1, "key");
    ASSERT_TRUE(opt_value);
    EXPECT_EQ(StringViewToUint8Vector("value"), opt_value.value());
  }
}

TEST_F(SessionStorageImplTest, DontRecreateOnRepeatedCommitFailure) {
  std::string namespace_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  std::optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));
  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  open_loop->Run();

  // Ensure that this database always fails to write data.
  session_storage_impl()->GetDatabaseForTesting().PostTaskWithThisObject(
      base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  EXPECT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();
  session_storage_impl()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();

        // Ensure that this database also always fails to write data.
        session_storage_impl()->GetDatabaseForTesting().AsyncCall(
            &DomStorageDatabase::MakeAllCommitsFailForTesting);
      }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  auto value = StringViewToUint8Vector("avalue");
  std::optional<std::vector<uint8_t>> old_value = std::nullopt;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringViewToUint8Vector("key"), value, old_value, "source",
              base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);

    old_value = value;
    value[0]++;
  }
  area.reset();

  // Wait for SessionStorageImpl to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  open_loop->Run();

  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  session_storage()->BindStorageArea(storage_key1, namespace_id,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  old_value = std::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringViewToUint8Vector("key"), value, old_value, "source",
              base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage_impl()->FlushAreaForTesting(namespace_id, storage_key1);

    old_value = value;
    value[0]++;
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());

  session_storage()->DeleteNamespace(namespace_id, false);
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, GetUsage) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  base::RunLoop loop;
  session_storage()->GetUsage(base::BindLambdaForTesting(
      [&](std::vector<mojom::SessionStorageUsageInfoPtr> usage) {
        loop.Quit();
        ASSERT_EQ(1u, usage.size());
        EXPECT_EQ(storage_key1, usage[0]->storage_key);
        EXPECT_EQ(namespace_id1, usage[0]->namespace_id);
      }));
  loop.Run();
}

TEST_F(SessionStorageImplTest, DeleteStorage) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  // First, test deleting data for a namespace that is open.
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  session_storage()->DeleteStorage(storage_key1, namespace_id1,
                                   base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Next, test that it deletes the data even if there isn't a namespace open.
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This re-initializes Session Storage, then deletes the storage.
  session_storage()->DeleteStorage(storage_key1, namespace_id1,
                                   base::DoNothing());

  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  data.clear();
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, PurgeInactiveWrappers) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  session_storage_impl()->FlushAreaForTesting(namespace_id1, storage_key1);

  area.reset();

  // Clear all the data from the backing database.
  base::RunLoop loop;
  session_storage_impl()->DatabaseForTesting()->RunDatabaseTask(
      base::BindOnce([](const DomStorageDatabase& db) {
        leveldb::WriteBatch batch;
        db.DeletePrefixed(StringViewToUint8Vector("map"), &batch);
        EXPECT_TRUE(db.Commit(&batch).ok());
        return 0;
      }),
      base::IgnoreArgs<int>(loop.QuitClosure()));
  loop.Run();

  // Now open many new wrappers (for different storage_keys) to trigger clean
  // up.
  for (int i = 1; i <= 100; ++i) {
    blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(
            base::StringPrintf("http://example.com:%d", i));
    session_storage()->BindStorageArea(storage_key, namespace_id1,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    RunUntilIdle();
    area.reset();
  }

  // And make sure caches were actually cleared.
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

// TODO(crbug.com/40650136): Flakes when verifying no data found.
TEST_F(SessionStorageImplTest, ClearDiskState) {
  SetBackingMode(SessionStorageImpl::BackingMode::kClearDiskStateOnOpen);
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace on disk.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace,
  // but it should have been deleted due to our backing mode.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should not be here, because SessionStorageImpl
  // clears disk space on open.
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedCloneWithDelete) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace which should be initialized and empty.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedCloneChainWithDelete) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n3;
  session_storage()->BindStorageArea(storage_key1, namespace_id3,
                                     area_n3.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n3.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedTripleCloneChain) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id4 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id3, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n4;
  session_storage()->BindStorageArea(storage_key1, namespace_id4,
                                     area_n4.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Trigger the populated of namespace 2 by deleting namespace 1.
  session_storage()->DeleteNamespace(namespace_id1, false);

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n4.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, TotalCloneChainDeletion) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id4 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);
  session_storage()->DeleteNamespace(namespace_id3, false);
  session_storage()->DeleteNamespace(namespace_id1, false);
  session_storage()->DeleteNamespace(namespace_id4, false);
}

}  // namespace

TEST_F(SessionStorageImplTest, PurgeMemoryDoesNotCrashOrHang) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  session_storage()->CreateNamespace(namespace_id2);
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value2"), std::nullopt,
                            "source1"));

  session_storage_impl()->FlushAreaForTesting(namespace_id1, storage_key1);

  area_n2.reset();

  RunUntilIdle();

  // Verify this doesn't crash or hang.
  session_storage_impl()->PurgeMemory();

  size_t memory_used = session_storage_impl()
                           ->GetNamespaceForTesting(namespace_id1)
                           ->storage_key_areas_[storage_key1]
                           ->data_map()
                           ->storage_area()
                           ->memory_used();
  EXPECT_EQ(0ul, memory_used);

  // Test the values is still there.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  std::optional<std::vector<uint8_t>> opt_value2 =
      DoTestGet(namespace_id2, storage_key1, "key1");
  ASSERT_TRUE(opt_value2);
  EXPECT_EQ(StringViewToUint8Vector("value2"), opt_value2.value());
}

TEST_F(SessionStorageImplTest, DeleteWithPersistBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Delete the storage_key namespace, but save it.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, DeleteWithoutPersistBeforeBrowserClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Delete the storage_key namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be gone, because the first namespace wasn't saved to disk.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, DeleteAfterCloneWithoutMojoClone) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://foobar.com");
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(storage_key1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringViewToUint8Vector("key1"),
                            StringViewToUint8Vector("value1"), std::nullopt,
                            "source1"));

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Delete the storage_key namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(storage_key1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be there, as the namespace should clone to all pending
  // namespaces on destruction if it didn't get a 'Clone' from mojo.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

// Regression test for https://crbug.com/1128318
TEST_F(SessionStorageImplTest, Bug1128318) {
  std::string namespace_id1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string namespace_id3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Create two namespaces by cloning.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  // And delete both namespaces again. Since namespace_id2 has child namespaces
  // that are waiting for a clone (namespace_id3), the delete will not complete
  // until the database has been initialized.
  session_storage()->DeleteNamespace(namespace_id2, false);
  session_storage()->DeleteNamespace(namespace_id3, false);
  // Now recreate one of the namespaces. The previous delete should have fully
  // completed before the namespace is recreated to prevent any dangling
  // references.
  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  RunUntilIdle();

  // At this point `namespace_id3` should be alive. It should not exist in meta
  // data yet, as that would only be populated once the actual clone happens.
  // As such, the namespace_entry for the namespace should be null.
  auto* ns = session_storage_impl()->GetNamespaceForTesting(namespace_id3);
  EXPECT_TRUE(ns);
  EXPECT_FALSE(base::Contains(session_storage_impl()
                                  ->GetMetadataForTesting()
                                  .namespace_storage_key_map(),
                              namespace_id3));
  EXPECT_EQ(ns->namespace_entry(), SessionStorageMetadata::NamespaceEntry());
}

}  // namespace storage
