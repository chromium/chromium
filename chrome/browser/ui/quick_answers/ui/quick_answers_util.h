// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"

namespace quick_answers {

// Constants.
inline constexpr int kContentHeaderWidth = 252;
inline constexpr int kContentTextWidth = 280;
inline constexpr char kGoogleSansFont[] = "Google Sans";
inline constexpr char kRobotoFont[] = "Roboto";

// |TypographyToken| values used by the Quick Answers cards.
enum class TypographyToken { kCrosBody2, kCrosButton2, kCrosTitle1 };

// Returns the |FontList| equivalents of |TypographyToken| values.
// This is so Quick Answers doesn't have an //ash/style dependency.
gfx::FontList GetFontList(TypographyToken token);

// Return the icon that corresponds to the Quick Answers result type.
const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type);

// Adds the list of |QuickAnswerUiElement| horizontally to the container.
// Returns the resulting container view.
views::View* AddHorizontalUiElements(
    views::View* container,
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements);

// Adds the list of |Views| horizontally to the container.
// Returns the resulting container view.
views::View* AddHorizontalViews(
    views::View* container,
    std::vector<std::unique_ptr<views::View>>& views);

// Creates a child view using FillLayout in the container. Uses |view| as the
// child view if it's specified, otherwise creates a new view.
// Returns the child view.
views::View* AddFillLayoutChildView(
    views::View* container,
    std::unique_ptr<views::View> view = std::make_unique<views::View>());

// Return the GURL that will link to the google search result for the
// query text.
GURL GetDetailsUrlForQuery(const std::string& query);

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
