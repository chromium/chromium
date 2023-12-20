// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_TEST_UTIL_H_

#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ash::assistant {

// Finds any descendents of |parent| with the desired |class_name| and pushes
// them onto the strongly typed |result| vector.
// NOTE: Callers are expected to ensure that casting to <T> makes sense. It is
// preferred to use the two argument variant of FindDescendentsOfClass() when
// possible for stronger type safety.
template <typename T>
void FindDescendentsOfClass(views::View* parent,
                            const std::string& class_name,
                            std::vector<T*>* result) {
  for (views::View* child : parent->children()) {
    if (child->GetClassName() == class_name)
      result->push_back(static_cast<T*>(child));
    FindDescendentsOfClass(child, class_name, result);
  }
}

// Finds any descendents of |parent| with class name equal to the static class
// variable |kViewClassName| and pushes them onto the strongly typed |result|
// vector.
// NOTE: This variant of FindDescendentsOfClass() is safer than the three
// argument variant and its usage should be preferred where possible.
template <typename T>
void FindDescendentsOfClass(views::View* parent, std::vector<T*>* result) {
  FindDescendentsOfClass(parent, T::kViewClassName, result);
}

}  // namespace ash::assistant

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_TEST_SUPPORT_TEST_UTIL_H_
