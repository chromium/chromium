// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_scrim_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Implementation of `ApcScrimManager` interface.
class ApcScrimManagerImpl : public ApcScrimManager,
                            public content::WebContentsObserver,
                            public views::ViewObserver {
 public:
  explicit ApcScrimManagerImpl(content::WebContents* web_contents);
  ApcScrimManagerImpl(const ApcScrimManagerImpl&) = delete;
  ApcScrimManagerImpl& operator=(const ApcScrimManagerImpl&) = delete;

  ~ApcScrimManagerImpl() override;

  void Show() override;
  void Hide() override;
  bool GetVisible() override;

 protected:
  // content::WebContentsObserver:
  // Protected so that it can be overridden by test class.
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  raw_ptr<views::View> GetContentsWebView();
  std::unique_ptr<views::View> CreateOverlayView();

  base::ScopedObservation<views::View, ViewObserver> observation_{this};
  raw_ptr<views::View> overlay_view_ref_ = nullptr;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_
