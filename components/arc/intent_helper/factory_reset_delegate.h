// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_

#include "components/arc/mojom/intent_helper.mojom.h"

namespace arc {

class FactoryResetDelegate {
 public:
  virtual ~FactoryResetDelegate() = default;

  // Does a reset of ARC; this wipes /data, and then re-calls on OOBE for
  // account binding to happen again, as if the user just went through OOBE.
  virtual void ResetArc() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_FACTORY_RESET_DELEGATE_H_
