// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_BLOB_TASK_PROXY_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_BLOB_TASK_PROXY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "storage/browser/blob/blob_storage_constants.h"

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
}  // namespace storage

namespace download {

// Proxy for blob related task on IO thread.
// Created on main thread and do work on IO thread, destroyed on IO thread.
class BlobTaskProxy {
 public:
  using BlobContextGetter =
      base::RepeatingCallback<base::WeakPtr<storage::BlobStorageContext>()>;
  using BlobDataHandleCallback =
      base::OnceCallback<void(std::unique_ptr<storage::BlobDataHandle>,
                              storage::BlobStatus status)>;

  static std::unique_ptr<BlobTaskProxy> Create(
      BlobContextGetter blob_context_getter,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  BlobTaskProxy(BlobContextGetter blob_context_getter,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~BlobTaskProxy();

  // Save blob data on UI thread. |callback| will be called on main thread after
  // blob construction completes.
  void SaveAsBlob(std::unique_ptr<std::string> data,
                  BlobDataHandleCallback callback);

 private:
  void InitializeOnIO(BlobContextGetter blob_context_getter);

  void SaveAsBlobOnIO(std::unique_ptr<std::string> data,
                      BlobDataHandleCallback callback);

  void BlobSavedOnIO(BlobDataHandleCallback callback,
                     storage::BlobStatus status);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Used to build blob data, must accessed on |io_task_runner_|.
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;

  // Used to access blob storage context.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // BlobDataHandle that will be eventually passed to main thread.
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;

  // Bounded to IO thread task runner.
  base::WeakPtrFactory<BlobTaskProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BlobTaskProxy);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_BLOB_TASK_PROXY_H_
