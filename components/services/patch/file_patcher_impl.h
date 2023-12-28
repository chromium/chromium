// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_
#define COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_

#include "base/files/file.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace base {
class File;
}  // namespace base

namespace patch {

class FilePatcherImpl : public mojom::FilePatcher {
 public:
  // This constructor assumes the FilePatcherImpl will be bound to an externally
  // owned receiver, such as through |mojo::MakeSelfOwnedReceiver()|.
  FilePatcherImpl();

  // Constructs a FilePatcherImpl bound to |receiver|.
  explicit FilePatcherImpl(mojo::PendingReceiver<mojom::FilePatcher> receiver);

  FilePatcherImpl(const FilePatcherImpl&) = delete;
  FilePatcherImpl& operator=(const FilePatcherImpl&) = delete;

  ~FilePatcherImpl() override;

 private:
  // patch::mojom::FilePatcher:
  void PatchFilePuffPatch(base::File input_file_path,
                          base::File patch_file_path,
                          base::File output_file_path,
                          PatchFilePuffPatchCallback callback) override;

  mojo::Receiver<mojom::FilePatcher> receiver_{this};
};

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_
