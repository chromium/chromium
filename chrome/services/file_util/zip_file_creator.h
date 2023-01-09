// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_
#define CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/services/file_util/public/mojom/zip_file_creator.mojom.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}

namespace zip {
struct Progress;
}

namespace chrome {

// Implementation of the ZipFileCreator Mojo service.
class ZipFileCreator : public base::RefCountedThreadSafe<ZipFileCreator>,
                       private chrome::mojom::ZipFileCreator {
 public:
  using PendingCreator = mojo::PendingReceiver<chrome::mojom::ZipFileCreator>;

  explicit ZipFileCreator(PendingCreator receiver);

  ZipFileCreator(const ZipFileCreator&) = delete;
  ZipFileCreator& operator=(const ZipFileCreator&) = delete;

  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

 private:
  friend class base::RefCountedThreadSafe<ZipFileCreator>;

  ~ZipFileCreator() override;

  using PendingListener = mojo::PendingRemote<chrome::mojom::ZipListener>;
  using PendingDirectory = mojo::PendingRemote<filesystem::mojom::Directory>;

  // chrome::mojom::ZipFileCreator:
  void CreateZipFile(PendingDirectory src_dir,
                     const std::vector<base::FilePath>& relative_paths,
                     base::File zip_file,
                     PendingListener listener) override;

  // Zips |src_dir| files given by |relative_paths| into |zip_file|.
  // Must be run in a separate task runner.
  void WriteZipFile(PendingDirectory src_dir,
                    const std::vector<base::FilePath>& relative_paths,
                    base::File zip_file,
                    PendingListener listener) const;

  using Listener = mojo::Remote<chrome::mojom::ZipListener>;

  // Progress handler.
  bool OnProgress(const Listener& listener,
                  const zip::Progress& progress) const;

  // Disconnection handler.
  void OnDisconnect();

  // Underlying ZipFileCreator receiver.
  mojo::Receiver<chrome::mojom::ZipFileCreator> receiver_;

  // Task runner for ZIP creation.
  using RunnerPtr = scoped_refptr<base::SequencedTaskRunner>;
  const RunnerPtr runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Flag used to cancel an ongoing ZIP creation.
  base::AtomicFlag cancelled_;
};

}  // namespace chrome

#endif  // CHROME_SERVICES_FILE_UTIL_ZIP_FILE_CREATOR_H_
