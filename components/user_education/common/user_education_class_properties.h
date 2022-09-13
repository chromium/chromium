// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CLASS_PROPERTIES_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CLASS_PROPERTIES_H_

#include "ui/base/class_property.h"

namespace user_education {

// Set to true for a UI elements that support ui::PropertyHandler if a help
// bubble is showing for that element. The element can respond however is
// appropriate, e.g. with a highlight or a color change.
//
// Individual help bubble [factory] implementations should set this value, as
// not all UI elements implement ui::PropertyHandler.
extern const ui::ClassProperty<bool>* const kHasInProductHelpPromoKey;

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CLASS_PROPERTIES_H_
