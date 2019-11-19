// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/blob_task_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace download {

// static
std::unique_ptr<BlobTaskProxy> BlobTaskProxy::Create(
    BlobContextGetter blob_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return std::make_unique<BlobTaskProxy>(blob_context_getter, io_task_runner);
}

BlobTaskProxy::BlobTaskProxy(
    BlobContextGetter blob_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner) {
  // Unretained the raw pointer because owner on UI thread should destroy this
  // object on IO thread.
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlobTaskProxy::InitializeOnIO,
                                base::Unretained(this), blob_context_getter));
}

BlobTaskProxy::~BlobTaskProxy() {
  io_task_runner_->BelongsToCurrentThread();
}

void BlobTaskProxy::InitializeOnIO(BlobContextGetter blob_context_getter) {
  io_task_runner_->BelongsToCurrentThread();
  blob_storage_context_ = blob_context_getter.Run();
}

void BlobTaskProxy::SaveAsBlob(std::unique_ptr<std::string> data,
                               BlobDataHandleCallback callback) {
  // Unretained the raw pointer because owner on UI thread should destroy this
  // object on IO thread.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BlobTaskProxy::SaveAsBlobOnIO, base::Unretained(this),
                     std::move(data), std::move(callback)));
}

void BlobTaskProxy::SaveAsBlobOnIO(std::unique_ptr<std::string> data,
                                   BlobDataHandleCallback callback) {
  io_task_runner_->BelongsToCurrentThread();

  // Build blob data. This has to do a copy into blob's internal storage.
  std::string blob_uuid = base::GenerateGUID();
  auto builder = std::make_unique<storage::BlobDataBuilder>(blob_uuid);
  builder->AppendData(*data);
  blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(std::move(builder));

  // Wait for blob data construction complete.
  auto cb = base::BindOnce(&BlobTaskProxy::BlobSavedOnIO,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  blob_data_handle_->RunOnConstructionComplete(std::move(cb));
}

void BlobTaskProxy::BlobSavedOnIO(BlobDataHandleCallback callback,
                                  storage::BlobStatus status) {
  io_task_runner_->BelongsToCurrentThread();

  // Relay BlobDataHandle and |status| back to main thread.
  auto cb =
      base::BindOnce(std::move(callback), std::move(blob_data_handle_), status);
  main_task_runner_->PostTask(FROM_HERE, std::move(cb));
}

}  // namespace download
