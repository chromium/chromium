// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/patch/patch_impl.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/patch/public/cpp/patch.h"
#include "components/update_client/update_client_errors.h"
#include "components/zucchini/zucchini.h"

namespace update_client {

namespace {

class PatcherImpl : public Patcher {
 public:
  explicit PatcherImpl(PatchChromiumFactory::Callback callback)
      : callback_(std::move(callback)) {}

  void PatchPuffPatch(base::File old_file,
                      base::File patch_file,
                      base::File destination_file,
                      PatchCompleteCallback callback) const override {
    patch::PuffPatch(callback_.Run(), std::move(old_file),
                     std::move(patch_file), std::move(destination_file),
                     std::move(callback));
  }

  void PatchZucchini(base::File old_file,
                     base::File patch_file,
                     base::File destination_file,
                     PatchCompleteCallback callback) const override {
    if (!old_file.IsValid()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    static_cast<int>(
                                        UnpackerError::kPatchInvalidOldFile)));
      return;
    }
    if (!patch_file.IsValid()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              static_cast<int>(UnpackerError::kPatchInvalidPatchFile)));
      return;
    }
    if (!destination_file.IsValid()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    static_cast<int>(
                                        UnpackerError::kPatchInvalidNewFile)));
      return;
    }
    patch::ZucchiniPatch(callback_.Run(), std::move(old_file),
                         std::move(patch_file), std::move(destination_file),
                         base::BindOnce([](zucchini::status::Code result) {
                           return static_cast<int>(result);
                         }).Then(std::move(callback)));
  }

 protected:
  ~PatcherImpl() override = default;

 private:
  const PatchChromiumFactory::Callback callback_;
};

}  // namespace

PatchChromiumFactory::PatchChromiumFactory(Callback callback)
    : callback_(std::move(callback)) {}

scoped_refptr<Patcher> PatchChromiumFactory::Create() const {
  return base::MakeRefCounted<PatcherImpl>(callback_);
}

PatchChromiumFactory::~PatchChromiumFactory() = default;

}  // namespace update_client
