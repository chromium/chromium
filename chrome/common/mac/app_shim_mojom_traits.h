// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_SHIM_MOJOM_TRAITS_H_
#define CHROME_COMMON_MAC_APP_SHIM_MOJOM_TRAITS_H_

#include <string>

#include "chrome/common/mac/app_shim.mojom-forward.h"
#include "components/variations/net/variations_command_line.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<chrome::mojom::FeatureStateDataView,
                    variations::VariationsCommandLine> {
  static const std::string& field_trial_states(
      const variations::VariationsCommandLine& v) {
    return v.field_trial_states;
  }
  static const std::string& field_trial_params(
      const variations::VariationsCommandLine& v) {
    return v.field_trial_params;
  }
  static const std::string& enable_features(
      const variations::VariationsCommandLine& v) {
    return v.enable_features;
  }
  static const std::string& disable_features(
      const variations::VariationsCommandLine& v) {
    return v.disable_features;
  }
  static bool Read(chrome::mojom::FeatureStateDataView data,
                   variations::VariationsCommandLine* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_MAC_APP_SHIM_MOJOM_TRAITS_H_
