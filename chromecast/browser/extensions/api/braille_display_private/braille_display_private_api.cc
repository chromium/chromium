// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/braille_display_private/braille_display_private_api.h"

namespace extensions {
namespace cast {
namespace api {
bool BrailleDisplayPrivateGetDisplayStateFunction::Prepare() {
  return true;
}

void BrailleDisplayPrivateGetDisplayStateFunction::Work() {
  braille_display_private::DisplayState state;
  state.available = false;
  state.text_column_count.reset(new int(0));
  state.text_row_count.reset(new int(0));
  SetResult(state.ToValue());
}

bool BrailleDisplayPrivateGetDisplayStateFunction::Respond() {
  return true;
}

BrailleDisplayPrivateWriteDotsFunction::
    BrailleDisplayPrivateWriteDotsFunction() {}

BrailleDisplayPrivateWriteDotsFunction::
    ~BrailleDisplayPrivateWriteDotsFunction() {}

bool BrailleDisplayPrivateWriteDotsFunction::Prepare() {
  params_ = braille_display_private::WriteDots::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);
  EXTENSION_FUNCTION_VALIDATE(
      params_->cells.size() >=
      static_cast<size_t>(params_->columns * params_->rows));
  return true;
}

void BrailleDisplayPrivateWriteDotsFunction::Work() {
  NOTIMPLEMENTED() << "BrailleDisplayPrivateWriteDotsFunction";
}

bool BrailleDisplayPrivateWriteDotsFunction::Respond() {
  return true;
}
}  // namespace api
}  // namespace cast
}  // namespace extensions
