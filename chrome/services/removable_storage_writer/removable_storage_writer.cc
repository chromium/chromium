// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/removable_storage_writer/removable_storage_writer.h"

#include <utility>

#include "base/files/file_path.h"

RemovableStorageWriter::RemovableStorageWriter(
    mojo::PendingReceiver<chrome::mojom::RemovableStorageWriter> receiver)
    : receiver_(this, std::move(receiver)) {}

RemovableStorageWriter::~RemovableStorageWriter() = default;

void RemovableStorageWriter::Write(
    const base::FilePath& source,
    const base::FilePath& target,
    mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client) {
  writer_.Write(source, target, std::move(client));
}

void RemovableStorageWriter::Verify(
    const base::FilePath& source,
    const base::FilePath& target,
    mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client) {
  writer_.Verify(source, target, std::move(client));
}
