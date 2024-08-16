// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_WARNING_VIEW_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

// A row which contains a warning message. It cannot be selected.
class PopupWarningView : public views::View {
  METADATA_HEADER(PopupWarningView, views::View)

 public:
  explicit PopupWarningView(const Suggestion& suggestion);
  ~PopupWarningView() override;

  PopupWarningView(const PopupWarningView&) = delete;
  PopupWarningView& operator=(const PopupWarningView&) = delete;

 private:
  const std::u16string text_value_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_WARNING_VIEW_H_
