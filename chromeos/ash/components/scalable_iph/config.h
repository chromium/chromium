// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_

#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

struct Config {
  Config();
  ~Config();
  int version_number;
  UiType ui_type;
  std::unique_ptr<ScalableIphDelegate::NotificationParams> notification_params;
  std::unique_ptr<ScalableIphDelegate::BubbleParams> bubble_params;
};

std::unique_ptr<Config> GetConfig(const base::Feature& feature);

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_CONFIG_H_
