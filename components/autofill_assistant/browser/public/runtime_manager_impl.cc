// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"

namespace autofill_assistant {

// static
RuntimeManagerImpl* RuntimeManagerImpl::GetForWebContents(
    content::WebContents* contents) {
  // |CreateForWebContents| does nothing if an instance is already attached.
  RuntimeManagerImpl::CreateForWebContents(contents);
  return RuntimeManagerImpl::FromWebContents(contents);
}

RuntimeManagerImpl::RuntimeManagerImpl(content::WebContents* web_contents) {}

RuntimeManagerImpl::RuntimeManagerImpl() = default;

RuntimeManagerImpl::~RuntimeManagerImpl() = default;

void RuntimeManagerImpl::AddObserver(RuntimeObserver* observer) {
  observers_.AddObserver(observer);
}

void RuntimeManagerImpl::RemoveObserver(RuntimeObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIState RuntimeManagerImpl::GetState() const {
  return ui_state_;
}

void RuntimeManagerImpl::SetUIState(UIState state) {
  if (state != ui_state_) {
    ui_state_ = state;
    for (RuntimeObserver& observer : observers_) {
      observer.OnStateChanged(ui_state_);
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RuntimeManagerImpl)
}  // namespace autofill_assistant
