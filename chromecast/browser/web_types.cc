// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/web_types.h"

namespace chromecast {

std::ostream& operator<<(std::ostream& os, PageState state) {
#define CASE(state)      \
  case PageState::state: \
    os << #state;        \
    return os;

  switch (state) {
    CASE(IDLE);
    CASE(LOADING);
    CASE(LOADED);
    CASE(CLOSED);
    CASE(DESTROYED);
    CASE(ERROR);
  }
#undef CASE
}

}  // namespace chromecast
