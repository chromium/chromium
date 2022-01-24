// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/file_stream_data_pipe_getter.h"

#include "net/base/net_errors.h"

namespace web_app {

// static
base::WeakPtr<FileStreamDataPipeGetter> FileStreamDataPipeGetter::Create(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver,
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t file_size,
    int buf_size) {
  FileStreamDataPipeGetter* self_owned =
      new FileStreamDataPipeGetter(std::move(receiver), std::move(context), url,
                                   offset, file_size, buf_size);
  return self_owned->weak_factory_.GetWeakPtr();
}

FileStreamDataPipeGetter::FileStreamDataPipeGetter(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver,
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t file_size,
    int buf_size)
    : context_(context),
      url_(url),
      offset_(offset),
      file_size_(file_size),
      buf_size_(buf_size) {
  DCHECK(url_.is_valid());
  receivers_.set_disconnect_with_reason_handler(base::BindRepeating(
      &FileStreamDataPipeGetter::OnDisconnect, weak_factory_.GetWeakPtr()));
  receivers_.Add(this, std::move(receiver));
}

FileStreamDataPipeGetter::~FileStreamDataPipeGetter() = default;

void FileStreamDataPipeGetter::Read(mojo::ScopedDataPipeProducerHandle pipe,
                                    ReadCallback callback) {
  std::move(callback).Run(net::OK, file_size_);

  DVLOG(1) << "Creating file stream adapter: " << url_.DebugString();
  DVLOG(1) << "Size: " << file_size_;

  stream_adapters_.push_back(
      base::MakeRefCounted<arc::ShareInfoFileStreamAdapter>(
          context_, url_, offset_, file_size_, buf_size_, std::move(pipe),
          base::BindOnce(&FileStreamDataPipeGetter::OnResult,
                         weak_factory_.GetWeakPtr())));
  stream_adapters_.back()->StartRunner();
}

void FileStreamDataPipeGetter::Clone(
    mojo::PendingReceiver<DataPipeGetter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FileStreamDataPipeGetter::OnResult(bool result) {
  DVLOG(1) << "FileStreamDataPipeGetter result: " << result;
}

void FileStreamDataPipeGetter::OnDisconnect(uint32_t custom_reason,
                                            const std::string& description) {
  DVLOG(1) << "FileStreamDataPipeGetter disconnect: " << custom_reason << " "
           << description;
  if (receivers_.empty()) {
    delete this;
  }
}

}  // namespace web_app
