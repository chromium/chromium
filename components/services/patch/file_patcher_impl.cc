// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/file_patcher_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "components/update_client/buildflags.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
// TODO(crbug.com/1349060) once Puffin patches are fully implemented,
// we should remove this #if
#include "third_party/puffin/src/include/puffin/puffpatch.h"
#endif

namespace patch {

FilePatcherImpl::FilePatcherImpl() = default;

FilePatcherImpl::FilePatcherImpl(
    mojo::PendingReceiver<mojom::FilePatcher> receiver)
    : receiver_(this, std::move(receiver)) {}

FilePatcherImpl::~FilePatcherImpl() = default;

// TODO(crbug.com/1349158): Remove this function once PatchFilePuffPatch is
// implemented as this becomes obsolete.
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

// TODO(crbug.com/1349158): Remove this function once PatchFilePuffPatch is
// implemented as this becomes obsolete.
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

void FilePatcherImpl::PatchFilePuffPatch(base::File input_file,
                                         base::File patch_file,
                                         base::File output_file,
                                         PatchFilePuffPatchCallback callback) {
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
  // we should remove this #if.
  const int patch_result_status = puffin::ApplyPuffPatch(
      std::move(input_file), std::move(patch_file), std::move(output_file));
  std::move(callback).Run(patch_result_status);
#else
  NOTREACHED();
#endif
}

}  // namespace patch
