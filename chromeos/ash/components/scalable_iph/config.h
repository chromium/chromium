// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

struct Config {
  Config();
  ~Config();
  int version_number = kCurrentVersionNumber;
  UiType ui_type;
  std::unique_ptr<ScalableIphDelegate::NotificationParams> notification_params;
  std::unique_ptr<ScalableIphDelegate::BubbleParams> bubble_params;

  // While you can put UI related config or any other config in `params`, usage
  // of `params` should be limited to custom conditions.
  base::flat_map<std::string, std::string> params;
};

std::unique_ptr<Config> GetConfig(const base::Feature& feature);

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_
