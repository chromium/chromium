// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_factory.h"

#include "build/build_config.h"
#include "components/input/stylus_handwriting_handler.h"

#if BUILDFLAG(IS_WIN)
#include "components/input/stylus_handwriting_handler_win.h"
#include "components/stylus_handwriting/win/features.h"
#endif

namespace input {

std::unique_ptr<StylusHandwritingHandler> CreateStylusHandwritingHandler(
    InputRouterClient* client) {
#if BUILDFLAG(IS_WIN)
  if (stylus_handwriting::win::IsStylusHandwritingWinEnabled() &&
      stylus_handwriting::win::kStylusHandwritingWinSmallGestures.Get()) {
    return std::make_unique<StylusHandwritingHandlerWin>(client);
  }
#endif
  return std::make_unique<StylusHandwritingHandler>(client);
}

}  // namespace input
