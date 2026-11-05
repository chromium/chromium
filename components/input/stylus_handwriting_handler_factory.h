// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_FACTORY_H_
#define COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_FACTORY_H_

#include <memory>

namespace input {

class InputRouterClient;
class StylusHandwritingHandler;

// Creates the platform-specific stylus handwriting handler.
std::unique_ptr<StylusHandwritingHandler> CreateStylusHandwritingHandler(
    InputRouterClient* client);

}  // namespace input

#endif  // COMPONENTS_INPUT_STYLUS_HANDWRITING_HANDLER_FACTORY_H_
