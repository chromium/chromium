// Copyright 2021 The Chromium Authors. All rights reserved.
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

webview::AsyncPageEvent_State ToGrpcPageState(PageState state) {
#define CASE(state)      \
  case PageState::state: \
    return webview::AsyncPageEvent_State_##state;

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
