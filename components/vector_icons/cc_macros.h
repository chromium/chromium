// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
#define COMPONENTS_VECTOR_ICONS_CC_MACROS_H_

#include <iterator>

// This file holds macros that are common to each vector icon target's
// vector_icons.cc.template file.

// The prefix is used to help make sure the string IDs are unique. Typically,
// matching the namespace of the icons should ensure that is the case. If the
// vector_icons.cc.template file doesn't define a prefix, we'll go without one.
#ifndef VECTOR_ICON_ID_PREFIX
#define VECTOR_ICON_ID_PREFIX ""
#endif

// This define may be specified by a vector icon target, allowing a compiler
// visibility attribute to be set on the icon symbol.
#ifndef VECTOR_ICON_EXPORT
#define VECTOR_ICON_EXPORT
#endif

#define VECTOR_ICON_REP_TEMPLATE(path_name, ...) \
  static constexpr gfx::PathElement path_name[] = {__VA_ARGS__};

#define VECTOR_ICON_TEMPLATE_CC(rep_list_name, icon_name, ...)         \
  constexpr char icon_name##Id[] = VECTOR_ICON_ID_PREFIX #icon_name;   \
  static constexpr gfx::VectorIconRep rep_list_name[] = {__VA_ARGS__}; \
  VECTOR_ICON_EXPORT constexpr gfx::VectorIcon icon_name = {           \
      rep_list_name, std::size(rep_list_name), icon_name##Id};

#else  // !COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
#error This file should only be included once.
#endif  // COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
