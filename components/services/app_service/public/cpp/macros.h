// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_

namespace apps {

#define SET_OPTIONAL_VALUE(VALUE) \
  if (delta->VALUE.has_value()) { \
    state->VALUE = delta->VALUE;  \
  }

#define SET_ENUM_VALUE(VALUE, DEFAULT_VALUE) \
  if (delta->VALUE != DEFAULT_VALUE) {       \
    state->VALUE = delta->VALUE;             \
  }

#define GET_VALUE(VALUE)           \
  if (delta_ && delta_->VALUE()) { \
    return delta_->VALUE();        \
  }                                \
  if (state_ && state_->VALUE()) { \
    return state_->VALUE();        \
  }                                \
  return nullptr;

#define IS_VALUE_CHANGED(VALUE)       \
  return delta_ && delta_->VALUE() && \
         (!state_ || (delta_->VALUE() != state_->VALUE()));

#define GET_VALUE_WITH_FALLBACK(VALUE, FALLBACK_VALUE) \
  if (delta_ && delta_->VALUE.has_value()) {           \
    return delta_->VALUE.value();                      \
  }                                                    \
  if (state_ && state_->VALUE.has_value()) {           \
    return state_->VALUE.value();                      \
  }                                                    \
  return FALLBACK_VALUE;

#define GET_VALUE_WITH_DEFAULT_VALUE(VALUE, DEFAULT_VALUE) \
  if (delta_ && delta_->VALUE != (DEFAULT_VALUE)) {        \
    return delta_->VALUE;                                  \
  }                                                        \
  if (state_) {                                            \
    return state_->VALUE;                                  \
  }                                                        \
  return DEFAULT_VALUE;

#define IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(VALUE, DEFAULT_VALUE) \
  return delta_ && (delta_->VALUE() != DEFAULT_VALUE) &&          \
         (!state_ || (delta_->VALUE() != state_->VALUE()));

#define GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(VALUE, CHECK, DEFAULT_RETURN) \
  if (delta_ && !delta_->VALUE.CHECK()) {                                     \
    return delta_->VALUE;                                                     \
  }                                                                           \
  if (state_ && !state_->VALUE.CHECK()) {                                     \
    return state_->VALUE;                                                     \
  }                                                                           \
  return DEFAULT_RETURN;

#define IS_VALUE_CHANGED_WITH_CHECK(VALUE, CHECK) \
  return delta_ && !delta_->VALUE().CHECK() &&    \
         (!state_ || (delta_->VALUE() != state_->VALUE()));

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MACROS_H_
