// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/print_params.h"

#include "components/printing/common/print.mojom.h"

namespace printing {

bool PrintMsgPrintParamsIsValid(const mojom::PrintParams& params) {
  return !params.content_size.IsEmpty() && !params.page_size.IsEmpty() &&
         !params.printable_area.IsEmpty() && params.document_cookie &&
         params.dpi.width() > kMinDpi && params.dpi.height() > kMinDpi &&
         params.margin_top >= 0 && params.margin_left >= 0;
}

}  // namespace printing
