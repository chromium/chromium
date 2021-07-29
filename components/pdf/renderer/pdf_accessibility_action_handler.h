// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_ACTION_HANDLER_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_ACTION_HANDLER_H_

struct PP_PdfAccessibilityActionData;

namespace pdf {

class PdfAccessibilityActionHandler {
 public:
  virtual ~PdfAccessibilityActionHandler() = default;

  virtual void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data) = 0;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_ACTION_HANDLER_H_
