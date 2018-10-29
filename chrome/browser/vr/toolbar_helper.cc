// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/toolbar_helper.h"

#include <memory>

#include "components/omnibox/browser/toolbar_model_impl.h"

class ToolbarModelDelegate;

namespace vr {

namespace {

// This number is arbitrary. For VR, use a number smaller than desktop's 32K, as
// the URL indicator does not show long URLs.
constexpr int kMaxURLDisplayChars = 1024;

}  // namespace

ToolbarHelper::ToolbarHelper(BrowserUiInterface* ui,
                             ToolbarModelDelegate* delegate)
    : ui_(ui),
      toolbar_model_(
          std::make_unique<ToolbarModelImpl>(delegate, kMaxURLDisplayChars)) {}

ToolbarHelper::~ToolbarHelper() {}

void ToolbarHelper::Update() {
  ToolbarState state(
      toolbar_model_->GetURL(), toolbar_model_->GetSecurityLevel(true),
      &toolbar_model_->GetVectorIcon(), toolbar_model_->ShouldDisplayURL(),
      toolbar_model_->IsOfflinePage());

  if (current_state_ == state)
    return;
  current_state_ = state;
  ui_->SetToolbarState(state);
}

}  // namespace vr
