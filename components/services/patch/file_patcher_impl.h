// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_
#define COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace patch {

class FilePatcherImpl : public mojom::FilePatcher {
 public:
  // This constructor assumes the FilePatcherImpl will be bound to an externally
  // owned receiver, such as through |mojo::MakeSelfOwnedReceiver()|.
  FilePatcherImpl();

  // Constructs a FilePatcherImpl bound to |receiver|.
  explicit FilePatcherImpl(mojo::PendingReceiver<mojom::FilePatcher> receiver);

  ~FilePatcherImpl() override;

 private:
  // patch::mojom::FilePatcher:
  void PatchFileBsdiff(base::File input_file,
                       base::File patch_file,
                       base::File output_file,
                       PatchFileBsdiffCallback callback) override;
  void PatchFileCourgette(base::File input_file,
                          base::File patch_file,
                          base::File output_file,
                          PatchFileCourgetteCallback callback) override;

  mojo::Receiver<mojom::FilePatcher> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FilePatcherImpl);
};

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_FILE_PATCHER_IMPL_H_
