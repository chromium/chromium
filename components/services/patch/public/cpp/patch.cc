// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/public/cpp/patch.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/update_client/component_patcher_operation.h"  // nogncheck
#include "mojo/public/cpp/bindings/remote.h"

namespace patch {

namespace {

class PatchParams : public base::RefCounted<PatchParams> {
 public:
  PatchParams(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
              PatchCallback callback)
      : file_patcher_(std::move(file_patcher)),
        callback_(std::move(callback)) {}

  mojo::Remote<mojom::FilePatcher>& file_patcher() { return file_patcher_; }

  PatchCallback TakeCallback() { return std::move(callback_); }

 private:
  friend class base::RefCounted<PatchParams>;

  ~PatchParams() = default;

  // The mojo::Remote<FilePatcher> is stored so it does not get deleted before
  // the callback runs.
  mojo::Remote<mojom::FilePatcher> file_patcher_;

  PatchCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PatchParams);
};

void PatchDone(scoped_refptr<PatchParams> params, int result) {
  params->file_patcher().reset();
  PatchCallback cb = params->TakeCallback();
  if (!cb.is_null())
    std::move(cb).Run(result);
}

}  // namespace

void Patch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
           const std::string& operation,
           const base::FilePath& input_path,
           const base::FilePath& patch_path,
           const base::FilePath& output_path,
           PatchCallback callback) {
  DCHECK(!callback.is_null());

  base::File input_file(input_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File patch_file(patch_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File output_file(output_path, base::File::FLAG_CREATE |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_EXCLUSIVE_WRITE);

  if (!input_file.IsValid() || !patch_file.IsValid() ||
      !output_file.IsValid()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*result=*/-1));
    return;
  }

  // In order to share |callback| between the connection error handler and the
  // FilePatcher calls, we have to use a context object.
  scoped_refptr<PatchParams> patch_params =
      new PatchParams(std::move(file_patcher), std::move(callback));

  patch_params->file_patcher().set_disconnect_handler(
      base::BindOnce(&PatchDone, patch_params, /*result=*/-1));

  if (operation == update_client::kBsdiff) {
    patch_params->file_patcher()->PatchFileBsdiff(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::BindOnce(&PatchDone, patch_params));
  } else if (operation == update_client::kCourgette) {
    patch_params->file_patcher()->PatchFileCourgette(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::BindOnce(&PatchDone, patch_params));
  } else {
    NOTREACHED();
  }
}

}  // namespace patch
