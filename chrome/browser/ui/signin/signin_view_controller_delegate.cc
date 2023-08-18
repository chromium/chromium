// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"

#include "base/observer_list.h"

SigninViewControllerDelegate::SigninViewControllerDelegate() = default;
SigninViewControllerDelegate::~SigninViewControllerDelegate() = default;

void SigninViewControllerDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void SigninViewControllerDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninViewControllerDelegate::NotifyModalDialogClosed() {
  for (auto& observer : observer_list_) {
    observer.OnModalDialogClosed();
  }
}
