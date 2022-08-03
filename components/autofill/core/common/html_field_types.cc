// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/common/html_field_types.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"

namespace autofill {

base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode) {
  switch (mode) {
    case HTML_MODE_NONE:
      return "";
    case HTML_MODE_BILLING:
      return "billing";
    case HTML_MODE_SHIPPING:
      return "shipping";
  }
  NOTREACHED();
}

}  // namespace autofill
