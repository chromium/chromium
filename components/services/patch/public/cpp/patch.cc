// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/public/cpp/patch.h"

#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace patch {

namespace {

class PatchParams : public base::RefCounted<PatchParams> {
 public:
  PatchParams(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
              base::OnceCallback<void(int result)> callback)
      : file_patcher_(std::move(file_patcher)),
        callback_(std::move(callback)) {}

  PatchParams(const PatchParams&) = delete;
  PatchParams& operator=(const PatchParams&) = delete;

  mojo::Remote<mojom::FilePatcher>& file_patcher() { return file_patcher_; }

  base::OnceCallback<void(int)> TakeCallback() { return std::move(callback_); }

 private:
  friend class base::RefCounted<PatchParams>;

  ~PatchParams() = default;

  // The mojo::Remote<FilePatcher> is stored so it does not get deleted before
  // the callback runs.
  mojo::Remote<mojom::FilePatcher> file_patcher_;

  base::OnceCallback<void(int)> callback_;
};

void PatchDone(scoped_refptr<PatchParams> params, int result) {
  params->file_patcher().reset();
  base::OnceCallback<void(int)> cb = params->TakeCallback();
  if (!cb.is_null())
    std::move(cb).Run(result);
}

}  // namespace

void PuffPatch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
               base::File input_file,
               base::File patch_file,
               base::File output_file,
               base::OnceCallback<void(int)> callback) {
  // Use a context object to share callback.
  scoped_refptr<PatchParams> patch_params = base::MakeRefCounted<PatchParams>(
      std::move(file_patcher), std::move(callback));

  patch_params->file_patcher().set_disconnect_handler(
      base::BindOnce(&PatchDone, patch_params, /*result=*/-1));

  patch_params->file_patcher()->PatchFilePuffPatch(
      std::move(input_file), std::move(patch_file), std::move(output_file),
      base::BindOnce(&PatchDone, patch_params));
}

void ZucchiniPatch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
                   base::File in,
                   base::File patch,
                   base::File out,
                   base::OnceCallback<void(zucchini::status::Code)> callback) {
  mojo::Remote<mojom::FilePatcher> patcher(std::move(file_patcher));
  auto* patcher_raw = patcher.get();
  patcher_raw->PatchFileZucchini(
      std::move(in), std::move(patch), std::move(out),
      base::BindOnce([](mojo::Remote<mojom::FilePatcher> remote,
                        zucchini::status::Code result) { return result; },
                     std::move(patcher))
          .Then(std::move(callback)));
}

}  // namespace patch
