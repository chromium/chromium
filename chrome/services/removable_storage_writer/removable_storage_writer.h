// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REMOVABLE_STORAGE_WRITER_REMOVABLE_STORAGE_WRITER_H_
#define CHROME_SERVICES_REMOVABLE_STORAGE_WRITER_REMOVABLE_STORAGE_WRITER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace base {
class FilePath;
}

class RemovableStorageWriter : public chrome::mojom::RemovableStorageWriter {
 public:
  explicit RemovableStorageWriter(
      mojo::PendingReceiver<chrome::mojom::RemovableStorageWriter> receiver);
  ~RemovableStorageWriter() override;

 private:
  // mojom::RemovableStorageWriter implementation:
  void Write(const base::FilePath& source,
             const base::FilePath& target,
             mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient>
                 client) override;

  void Verify(const base::FilePath& source,
              const base::FilePath& target,
              mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient>
                  client) override;

  mojo::Receiver<chrome::mojom::RemovableStorageWriter> receiver_;
  image_writer::ImageWriterHandler writer_;

  DISALLOW_COPY_AND_ASSIGN(RemovableStorageWriter);
};

#endif  // CHROME_SERVICES_REMOVABLE_STORAGE_WRITER_REMOVABLE_STORAGE_WRITER_H_
