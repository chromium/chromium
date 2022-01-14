// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.h"

#include <cmath>

#include "base/files/file_path.h"

namespace app_list {

FtrlOptimizer::FtrlOptimizer(const base::FilePath& path,
                             const Params& params,
                             std::vector<std::unique_ptr<FtrlExpert>>&& experts)
    : params_(params),
      experts_(std::move(experts)),
      proto_(path, params.write_delay) {
  proto_.RegisterOnRead(
      base::BindOnce(&FtrlOptimizer::OnProtoRead, weak_factory_.GetWeakPtr()));
  proto_.Init();
}

FtrlOptimizer::~FtrlOptimizer() {}

std::vector<double> FtrlOptimizer::Score(
    const std::vector<std::string>& items) {
  // TODO(crbug.com/1199206): Unimplemented.
  return {0.0};
}

void FtrlOptimizer::Train(const std::string& item) {
  // TODO(crbug.com/1199206): Unimplemented.
}

void FtrlOptimizer::OnProtoRead(ReadStatus status) {}

}  // namespace app_list
