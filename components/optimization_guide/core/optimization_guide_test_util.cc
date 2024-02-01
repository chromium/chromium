// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_test_util.h"

#include "base/base64.h"
#include "build/build_config.h"

namespace optimization_guide {

#if BUILDFLAG(IS_WIN)
const char kTestAbsoluteFilePath[] = "C:\\absolute\\file\\path";
const char kTestRelativeFilePath[] = "relative\\file\\path";
#else
const char kTestAbsoluteFilePath[] = "/absolutefilepath";
const char kTestRelativeFilePath[] = "relativefilepath";
#endif

std::string CreateHintsConfig(
    const GURL& hints_url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::proto::Any* metadata) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key(hints_url.host());
  hint->set_key_representation(optimization_guide::proto::HOST);

  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern(hints_url.path().substr(1));

  optimization_guide::proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(optimization_type);
  if (metadata)
    *optimization->mutable_any_metadata() = *metadata;

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  return base::Base64Encode(encoded_config);
}

}  // namespace optimization_guide
