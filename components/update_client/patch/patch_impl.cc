// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/patch/patch_impl.h"

#include "components/services/patch/public/cpp/patch.h"
#include "components/update_client/component_patcher_operation.h"

namespace update_client {

namespace {

class PatcherImpl : public Patcher {
 public:
  explicit PatcherImpl(PatchChromiumFactory::Callback callback)
      : callback_(std::move(callback)) {}

  void PatchBsdiff(const base::FilePath& old_file,
                   const base::FilePath& patch_file,
                   const base::FilePath& destination,
                   PatchCompleteCallback callback) const override {
    patch::Patch(callback_.Run(), update_client::kBsdiff, old_file, patch_file,
                 destination, std::move(callback));
  }

  void PatchCourgette(const base::FilePath& old_file,
                      const base::FilePath& patch_file,
                      const base::FilePath& destination,
                      PatchCompleteCallback callback) const override {
    patch::Patch(callback_.Run(), update_client::kCourgette, old_file,
                 patch_file, destination, std::move(callback));
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
