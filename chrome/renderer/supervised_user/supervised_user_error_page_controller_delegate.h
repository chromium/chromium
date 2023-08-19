// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_
#define CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_

#include "base/functional/callback_forward.h"

// Called when the interstitial calls the installed JS methods.
class SupervisedUserErrorPageControllerDelegate {
 public:
  // A callback that indicates whether the URL access request was initiated
  // successfully.
  using UrlAccessRequestInitiated = base::OnceCallback<void(bool)>;

  // Called to go to the previous page after the remote URL approval request has
  // been sent.
  virtual void GoBack() = 0;

  // Called to send remote URL approval request.
  virtual void RequestUrlAccessRemote(UrlAccessRequestInitiated callback) = 0;

  // Called to initiate local URL approval flow.
  virtual void RequestUrlAccessLocal(UrlAccessRequestInitiated callback) = 0;

 protected:
  virtual ~SupervisedUserErrorPageControllerDelegate() {}
};

#endif  // CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_
