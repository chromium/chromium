// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_IMAGE_VIEW_H_

#include <memory>

#include "ui/views/controls/image_view.h"

namespace autofill_ai {

std::unique_ptr<views::ImageView> CreateSmallAutofillAiIconImageView();
std::unique_ptr<views::ImageView> CreateLargeAutofillAiIconImageView();

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_IMAGE_VIEW_H_
