// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_COMMON_PRINT_PARAMS_H_
#define COMPONENTS_PRINTING_COMMON_PRINT_PARAMS_H_

#include "components/printing/common/print.mojom-forward.h"

namespace printing {

constexpr double kMinDpi = 1.0;

bool PrintMsgPrintParamsIsValid(const mojom::PrintParams& params);

}  // namespace printing

#endif  // COMPONENTS_PRINTING_COMMON_PRINT_PARAMS_H_
