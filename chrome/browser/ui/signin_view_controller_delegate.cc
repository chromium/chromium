// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller_delegate.h"

SigninViewControllerDelegate::SigninViewControllerDelegate() = default;
SigninViewControllerDelegate::~SigninViewControllerDelegate() = default;

void SigninViewControllerDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void SigninViewControllerDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninViewControllerDelegate::NotifyModalSigninClosed() {
  for (auto& observer : observer_list_)
    observer.OnModalSigninClosed();
}
