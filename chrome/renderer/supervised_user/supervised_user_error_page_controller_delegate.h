// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_
#define CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

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

#if BUILDFLAG(IS_ANDROID)
  // Called to open the learn more page for the user.
  virtual void LearnMore(base::OnceClosure open_help_page) = 0;
#endif  // BUILDFLAG(IS_ANDROID)

 protected:
  virtual ~SupervisedUserErrorPageControllerDelegate() = default;
};

#endif  // CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_H_
