// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_IPC_DATA_SOURCE_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_IPC_DATA_SOURCE_H_

#include <stdint.h>

#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "media/base/data_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class TaskRunner;
}

// Provides the metadata parser with blob data from the browser process. Class
// must be created and destroyed on the utility thread. Class may be used as a
// DataSource on a different thread. The utility thread must not be blocked
// for read operations to succeed.
class IPCDataSource : public media::DataSource {
 public:
  // May only be called on the utility thread.
  IPCDataSource(
      mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
      int64_t total_size);
  ~IPCDataSource() override;

  // media::DataSource implementation. The methods may be called on any single
  // thread. First usage of these methods attaches a thread checker.
  void Stop() override;
  void Abort() override;
  void Read(int64_t position,
            int size,
            uint8_t* destination,
            const ReadCB& callback) override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

 private:
  // Media data read helpers: must be run on the utility thread.
  void ReadMediaData(uint8_t* destination,
                     const ReadCB& callback,
                     int64_t position,
                     int size);
  void ReadDone(uint8_t* destination,
                const ReadCB& callback,
                const std::vector<uint8_t>& data);

  mojo::Remote<chrome::mojom::MediaDataSource> media_data_source_;
  const int64_t total_size_;

  scoped_refptr<base::TaskRunner> utility_task_runner_;

  THREAD_CHECKER(utility_thread_checker_);

  // Enforces that the DataSource methods are called on one other thread only.
  THREAD_CHECKER(data_source_thread_checker_);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_IPC_DATA_SOURCE_H_
