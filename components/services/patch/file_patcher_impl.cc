// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/file_patcher_impl.h"

#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace patch {

FilePatcherImpl::FilePatcherImpl() = default;

FilePatcherImpl::FilePatcherImpl(
    mojo::PendingReceiver<mojom::FilePatcher> receiver)
    : receiver_(this, std::move(receiver)) {}

FilePatcherImpl::~FilePatcherImpl() = default;

void FilePatcherImpl::PatchFileBsdiff(base::File input_file,
                                      base::File patch_file,
                                      base::File output_file,
                                      PatchFileBsdiffCallback callback) {
  DCHECK(input_file.IsValid());
  DCHECK(patch_file.IsValid());
  DCHECK(output_file.IsValid());

  const int patch_result_status = bsdiff::ApplyBinaryPatch(
      std::move(input_file), std::move(patch_file), std::move(output_file));
  std::move(callback).Run(patch_result_status);
}

void FilePatcherImpl::PatchFileCourgette(base::File input_file,
                                         base::File patch_file,
                                         base::File output_file,
                                         PatchFileCourgetteCallback callback) {
  DCHECK(input_file.IsValid());
  DCHECK(patch_file.IsValid());
  DCHECK(output_file.IsValid());

  const int patch_result_status = courgette::ApplyEnsemblePatch(
      std::move(input_file), std::move(patch_file), std::move(output_file));
  std::move(callback).Run(patch_result_status);
}

}  // namespace patch
