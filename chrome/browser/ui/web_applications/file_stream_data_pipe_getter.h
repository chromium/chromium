// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_STREAM_DATA_PIPE_GETTER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_STREAM_DATA_PIPE_GETTER_H_

#include <cstdint>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace web_app {

class FileStreamDataPipeGetter : public network::mojom::DataPipeGetter {
 public:
  FileStreamDataPipeGetter(scoped_refptr<storage::FileSystemContext> context,
                           const storage::FileSystemURL& url,
                           int64_t offset,
                           int64_t file_size,
                           int buf_size);
  FileStreamDataPipeGetter(const FileStreamDataPipeGetter& other) = delete;
  FileStreamDataPipeGetter& operator=(const FileStreamDataPipeGetter&) = delete;
  ~FileStreamDataPipeGetter() override;

 private:
  // network::mojom::DataPipeGetter:
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override;
  void Clone(mojo::PendingReceiver<DataPipeGetter> receiver) override;

  void OnResult(bool result);

  const scoped_refptr<storage::FileSystemContext> context_;
  const storage::FileSystemURL url_;
  const int64_t offset_;
  const int64_t file_size_;
  const int buf_size_;

  mojo::ReceiverSet<network::mojom::DataPipeGetter> receivers_;
  scoped_refptr<arc::ShareInfoFileStreamAdapter> stream_adapter_;
  base::WeakPtrFactory<FileStreamDataPipeGetter> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_STREAM_DATA_PIPE_GETTER_H_
