// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_TEMPORARY_FILE_GETTER_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_TEMPORARY_FILE_GETTER_H_

#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class TemporaryFileGetter : public chrome::mojom::TemporaryFileGetter {
 public:
  // chrome::mojom::TemporaryFileGetter:
  TemporaryFileGetter();

  TemporaryFileGetter(const TemporaryFileGetter&) = delete;
  TemporaryFileGetter& operator=(const TemporaryFileGetter&) = delete;

  ~TemporaryFileGetter() override;

  // Requests a temporary file. If the request is accepted, `temp_file`
  // will be returned through the callback with a valid file descriptor.
  // If the request is rejected, `temp_file` will be invalid.
  void RequestTemporaryFile(RequestTemporaryFileCallback callback) override;

  mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
  GetRemoteTemporaryFileGetter() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  int num_files_requested_ = 0;
  mojo::Receiver<chrome::mojom::TemporaryFileGetter> receiver_{this};
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_TEMPORARY_FILE_GETTER_H_
