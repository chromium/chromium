// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_STORE_FILE_TASK_H_
#define CHROME_BROWSER_WEBSHARE_STORE_FILE_TASK_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

// Stores shared |file| using the specified |filename|.
class StoreFileTask : public blink::mojom::BlobReaderClient {
 public:
  // |available_space| will be decreased by the size of the file.
  StoreFileTask(base::FilePath filename,
                blink::mojom::SharedFilePtr file,
                uint64_t& available_space,
                blink::mojom::ShareService::ShareCallback callback);
  StoreFileTask(const StoreFileTask&) = delete;
  StoreFileTask& operator=(const StoreFileTask&) = delete;
  ~StoreFileTask() override;

  // Must be called on a thread that allows blocking IO.
  void Start();

  // Create empty files instead of copying from the SharedFilePtr.
  static void SkipCopyingForTesting();

 private:
  void StartRead();
  void OnDataPipeReadable(MojoResult result);
  void OnSuccess();

  // mojom::blink::BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override;

  base::FilePath filename_;
  blink::mojom::SharedFilePtr file_;
  const raw_ref<uint64_t> available_space_;
  blink::mojom::ShareService::ShareCallback callback_;
  base::File output_file_;

  uint64_t total_bytes_ = 0;
  uint64_t bytes_received_ = 0;
  bool received_all_data_ = false;
  bool received_on_complete_ = false;

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher read_pipe_watcher_;
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_{this};
  base::WeakPtrFactory<StoreFileTask> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_STORE_FILE_TASK_H_
