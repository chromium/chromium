// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_TOGGLE_TRACKED_ELEMENT_ATTENTION_UTILS_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_TOGGLE_TRACKED_ELEMENT_ATTENTION_UTILS_H_

namespace views {
class View;
}

namespace user_education {

void MaybeRemoveAttentionStateFromTrackedElement(views::View* tracked_element);
void MaybeApplyAttentionStateToTrackedElement(views::View* tracked_element);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_TOGGLE_TRACKED_ELEMENT_ATTENTION_UTILS_H_
