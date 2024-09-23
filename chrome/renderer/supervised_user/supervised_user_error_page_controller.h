// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_H_
#define CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gin/wrappable.h"

namespace content {
class RenderFrame;
}

class SupervisedUserErrorPageControllerDelegate;

// This class makes various helper functions available to supervised user
// interstitials when committed interstitials are on. It is bound to the
// JavaScript window.certificateErrorPageController object.
class SupervisedUserErrorPageController
    : public gin::Wrappable<SupervisedUserErrorPageController> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  SupervisedUserErrorPageController(const SupervisedUserErrorPageController&) =
      delete;
  SupervisedUserErrorPageController& operator=(
      const SupervisedUserErrorPageController&) = delete;

  // Will invoke methods on |delegate| in response to user actions taken on the
  // interstitial. May call delegate methods even after the page has been
  // navigated away from, so it is recommended consumers make sure the weak
  // pointers are destroyed in response to navigations.
  static void Install(
      content::RenderFrame* render_frame,
      base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate);

 private:
  SupervisedUserErrorPageController(
      base::WeakPtr<SupervisedUserErrorPageControllerDelegate> delegate,
      content::RenderFrame* render_frame);
  ~SupervisedUserErrorPageController() override;

  void GoBack();
  void RequestUrlAccessRemote();
  void RequestUrlAccessLocal();

  void OnRequestUrlAccessRemote(bool success);

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  base::WeakPtr<SupervisedUserErrorPageControllerDelegate> const delegate_;

  raw_ptr<content::RenderFrame, DanglingUntriaged> render_frame_;

  // This weak factory is used to generate weak pointers to the controller that
  // are used for the request permission callback, so messages to no longer
  // existing interstitials are ignored.
  base::WeakPtrFactory<SupervisedUserErrorPageController> weak_factory_{this};
};

#endif  // CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_H_
