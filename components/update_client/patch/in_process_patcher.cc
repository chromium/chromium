// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/patch/in_process_patcher.h"

#include <utility>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

namespace {

class InProcessPatcher : public Patcher {
 public:
  InProcessPatcher() = default;

  void PatchPuffPatch(base::File input_file,
                      base::File patch_file,
                      base::File output_file,
                      PatchCompleteCallback callback) const override {
    std::move(callback).Run(puffin::ApplyPuffPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

 protected:
  ~InProcessPatcher() override = default;
};

}  // namespace

InProcessPatcherFactory::InProcessPatcherFactory() = default;

scoped_refptr<Patcher> InProcessPatcherFactory::Create() const {
  return base::MakeRefCounted<InProcessPatcher>();
}

InProcessPatcherFactory::~InProcessPatcherFactory() = default;

}  // namespace update_client
