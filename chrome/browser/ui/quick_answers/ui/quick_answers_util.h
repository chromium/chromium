// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/vector_icons/vector_icons.h"

namespace quick_answers {

// Return the icon that corresponds to the Quick Answers result type.
const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type);

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
