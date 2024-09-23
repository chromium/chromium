// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/test_api/test_api.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/immediate_crash.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "components/services/storage/test_api_stubs.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace storage {

namespace {

class TestApiDatabaseEnv : public leveldb_env::ChromiumEnv {
 public:
  TestApiDatabaseEnv() : ChromiumEnv(CreateFilesystemProxy()) {}
  TestApiDatabaseEnv(const TestApiDatabaseEnv&) = delete;
  TestApiDatabaseEnv& operator=(const TestApiDatabaseEnv&) = delete;
};

TestApiDatabaseEnv* GetTestApiDatabaseEnv() {
  static base::NoDestructor<TestApiDatabaseEnv> env;
  return env.get();
}

void CreateAndCompactDatabase(
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    mojom::TestApi::ForceLeveldbDatabaseCompactionCallback callback) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.env = GetTestApiDatabaseEnv();
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, name, &db);
  CHECK(status.ok()) << status.ToString();
  db->CompactRange(nullptr, nullptr);
  db.reset();
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

class TestApiImpl : public mojom::TestApi {
 public:
  TestApiImpl() = default;
  ~TestApiImpl() override = default;

  void AddReceiver(mojo::PendingReceiver<mojom::TestApi> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::TestApi implementation:
  void CrashNow() override { base::ImmediateCrash(); }

  void ForceLeveldbDatabaseCompaction(
      const std::string& name,
      ForceLeveldbDatabaseCompactionCallback callback) override {
    // Note that we post to a SequencedTaskRunner because the task will use Mojo
    // bindings, and by default Mojo bindings assume there is a current
    // SequencedTaskRunner::CurrentDefaultHandle they can use for scheduling.
    base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&CreateAndCompactDatabase, name,
                           base::SequencedTaskRunner::GetCurrentDefault(),
                           std::move(callback)));
  }

 private:
  mojo::ReceiverSet<mojom::TestApi> receivers_;
};

void BindTestApi(mojo::ScopedMessagePipeHandle test_api_receiver) {
  static base::NoDestructor<TestApiImpl> impl;
  impl->AddReceiver(
      mojo::PendingReceiver<mojom::TestApi>(std::move(test_api_receiver)));
}

}  // namespace

void InjectTestApiImplementation() {
  SetTestApiBinderForTesting(base::BindRepeating(&BindTestApi));
}

}  // namespace storage
