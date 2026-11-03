// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button.h"

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"

MediaToolbarButton::MediaToolbarButton() = default;

MediaToolbarButton::~MediaToolbarButton() = default;

void MediaToolbarButton::AddObserver(MediaToolbarButtonObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaToolbarButton::RemoveObserver(MediaToolbarButtonObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaToolbarButton::NotifyMediaDialogOpened() {
  observers_.Notify(&MediaToolbarButtonObserver::OnMediaDialogOpened);
}

void MediaToolbarButton::NotifyMediaButtonShown() {
  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonShown);
}

void MediaToolbarButton::NotifyMediaButtonHidden() {
  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonHidden);
}

void MediaToolbarButton::NotifyMediaButtonEnabled() {
  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonEnabled);
}

void MediaToolbarButton::NotifyMediaButtonDisabled() {
  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonDisabled);
}
