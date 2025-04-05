// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"

#include "content/public/browser/web_contents.h"

namespace page_actions {

FakeTabInterface::FakeTabInterface(content::WebContents* contents)
    : contents_(contents) {}

FakeTabInterface::~FakeTabInterface() = default;

base::CallbackListSubscription FakeTabInterface::RegisterDidActivate(
    base::RepeatingCallback<void(TabInterface*)> cb) {
  return activation_callbacks_.Add(cb);
}

base::CallbackListSubscription FakeTabInterface::RegisterWillDeactivate(
    base::RepeatingCallback<void(TabInterface*)> cb) {
  return deactivation_callbacks_.Add(cb);
}

void FakeTabInterface::Activate() {
  is_activated_ = true;
  activation_callbacks_.Notify(this);
}

void FakeTabInterface::Deactivate() {
  is_activated_ = false;
  deactivation_callbacks_.Notify(this);
}

bool FakeTabInterface::IsActivated() const {
  return is_activated_;
}

}  // namespace page_actions
