// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_
#define COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_

#include "base/functional/callback_forward.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/zucchini/zucchini.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace base {
class File;
}

namespace patch {

// Patches `in` with `patch` using Puffin and writes the output to `out`.
// `callback` will be run on the same sequence.
void PuffPatch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
               base::File in,
               base::File patch,
               base::File out,
               base::OnceCallback<void(int result)> callback);

// Patches `in` with `patch` using Zucchini and writes the output to `out`.
// `callback` will be run on the same sequence.
void ZucchiniPatch(
    mojo::PendingRemote<mojom::FilePatcher> file_patcher,
    base::File input_abs_path,
    base::File patch_abs_path,
    base::File output_abs_path,
    base::OnceCallback<void(zucchini::status::Code result)> callback);

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_
