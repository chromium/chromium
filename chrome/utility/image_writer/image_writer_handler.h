// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_
#define CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/utility/image_writer/image_writer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}

namespace image_writer {

class ImageWriterHandler {
 public:
  ImageWriterHandler();
  ~ImageWriterHandler();

  void Write(
      const base::FilePath& image,
      const base::FilePath& device,
      mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client);
  void Verify(
      const base::FilePath& image,
      const base::FilePath& device,
      mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client);

  // Methods for sending the different messages back to the |client_|.
  // Generally should be called by image_writer::ImageWriter.
  virtual void SendProgress(int64_t progress);
  virtual void SendSucceeded();
  virtual void SendFailed(const std::string& error);
  virtual void SendCancelled() {}

 private:
  void Cancel();

  bool ShouldResetImageWriter(const base::FilePath& image,
                              const base::FilePath& device);

  mojo::Remote<chrome::mojom::RemovableStorageWriterClient> client_;
  std::unique_ptr<ImageWriter> image_writer_;

  DISALLOW_COPY_AND_ASSIGN(ImageWriterHandler);
};

}  // namespace image_writer

#endif  // CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_
