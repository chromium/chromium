// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/score_normalizer.h"

namespace app_list {
namespace {

// This should be incremented whenever a change to the algorithm is made that
// is incompatible with on-disk state. On reading, any state is wiped if its
// version doesn't match.
const int32_t kModelVersion = 1;

}  // namespace

ScoreNormalizer::ScoreNormalizer(const base::FilePath& filepath,
                                 const Params& params)
    : params_(params) {
  proto_.Init(
      filepath, params.write_delay,
      base::BindOnce(&ScoreNormalizer::OnProtoRead, weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

ScoreNormalizer::~ScoreNormalizer() {}

void ScoreNormalizer::OnProtoRead(ReadStatus status) {
  DCHECK(proto_.initialized());

  // TODO(crbug.com/1199206): Add error metrics based on |status|.

  if ((proto_->has_model_version() &&
       proto_->model_version() != kModelVersion) ||
      (proto_->has_parameter_version() &&
       proto_->parameter_version() != params_.version)) {
    proto_.Purge();
  }

  proto_->set_model_version(kModelVersion);
  proto_->set_parameter_version(params_.version);
}

}  // namespace app_list
