// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace views {
class StyledLabel;
class Textfield;
}  // namespace views

namespace autofill {

// Defines a title view with an icon, a separator, and a label, to be used
// by dialogs that need to present the Google Pay logo with a separator and
// custom horizontal padding.
class TitleWithIconAndSeparatorView : public views::View {
 public:
  explicit TitleWithIconAndSeparatorView(const base::string16& window_title);
  ~TitleWithIconAndSeparatorView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Creates and returns a small Textfield intended to be used for CVC entry.
std::unique_ptr<views::Textfield> CreateCvcTextfield();

// Defines a view with legal message. This class handles the legal message
// parsing and the links clicking events.
class LegalMessageView : public views::View {
 public:
  explicit LegalMessageView(const LegalMessageLines& legal_message_lines,
                            views::StyledLabelListener* listener);
  ~LegalMessageView() override;

  const GURL GetUrlForLink(views::StyledLabel* label, const gfx::Range& range);

 private:
  std::unique_ptr<views::StyledLabel> CreateLegalMessageLineLabel(
      const LegalMessageLine& line,
      views::StyledLabelListener* listener);

  LegalMessageLines legal_message_lines_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
