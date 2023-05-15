// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
#define COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/ced/src/util/encodings/encodings.h"

namespace base {
class FilePath;
}

namespace unzip {

// Unzips |zip_file| into |output_dir|.
using UnzipCallback = base::OnceCallback<void(bool result)>;
void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           UnzipCallback result_callback);

// Similar to |Unzip| but only unzips files that |filter_callback| vetted.
// Note that |filter_callback| may be invoked from a background thread.
using UnzipFilterCallback =
    base::RepeatingCallback<bool(const base::FilePath& path)>;
void UnzipWithFilter(mojo::PendingRemote<mojom::Unzipper> unzipper,
                     const base::FilePath& zip_file,
                     const base::FilePath& output_dir,
                     UnzipFilterCallback filter_callback,
                     UnzipCallback result_callback);

using UnzipListenerCallback = base::RepeatingCallback<void(uint64_t bytes)>;
void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           mojom::UnzipOptionsPtr options,
           UnzipListenerCallback listener_callback,
           UnzipCallback result_callback);

using DetectEncodingCallback = base::OnceCallback<void(Encoding)>;
void DetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                    const base::FilePath& zip_file,
                    DetectEncodingCallback result_callback);

using GetExtractedInfoCallback = base::OnceCallback<void(mojom::InfoPtr)>;
void GetExtractedInfo(mojo::PendingRemote<mojom::Unzipper> unzipper,
                      const base::FilePath& zip_file,
                      GetExtractedInfoCallback result_callback);

class UnzipParams;

// Class that wraps the unzip service to manage the lifetime of its
// mojo conncections to enable cancellation, etc.
class ZipFileUnpacker : public base::RefCountedThreadSafe<ZipFileUnpacker> {
 public:
  ZipFileUnpacker();
  void Unpack(mojo::PendingRemote<mojom::Unzipper> unzipper,
              const base::FilePath& zip_file,
              const base::FilePath& output_dir,
              mojom::UnzipOptionsPtr options,
              UnzipListenerCallback listener_callback,
              UnzipCallback result_callback);

  void Stop();

  bool CleanUpDone();

  void CleanUp();

 private:
  friend class base::RefCountedThreadSafe<ZipFileUnpacker>;

  ~ZipFileUnpacker();

  const scoped_refptr<base::SequencedTaskRunner> runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  base::File zip_file_;
  scoped_refptr<UnzipParams> params_;
  mojo::PendingRemote<unzip::mojom::UnzipFilter> filter_remote_;
  mojo::PendingRemote<unzip::mojom::UnzipListener> listener_remote_;
};

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
