// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

// A dialog allowing the user to complete a flow (e.g. signing in to an identity
// provider) prompted by FedCM.
// TODO(crbug.com/1430830): Rename modal dialog to pop-up window.
class FedCmModalDialogView : public content::WebContentsObserver {
 public:
  class Observer {
   public:
    // Tells observers that the pop-up window is destroyed.
    virtual void OnPopupWindowDestroyed() = 0;
  };

  explicit FedCmModalDialogView(content::WebContents* web_contents,
                                FedCmModalDialogView::Observer* observer);
  FedCmModalDialogView(const FedCmModalDialogView&) = delete;
  FedCmModalDialogView& operator=(const FedCmModalDialogView&) = delete;
  ~FedCmModalDialogView() override;

  // Shows a modal dialog of |url|. The |url| is commonly but not limited to a
  // URL which allows the user to sign in with an identity provider.
  virtual content::WebContents* ShowPopupWindow(const GURL& url);
  virtual void ClosePopupWindow();

  // content::WebContentsObserver
  void WebContentsDestroyed() override;

 private:
  raw_ptr<content::WebContents> source_window_{nullptr};
  raw_ptr<content::WebContents> popup_window_{nullptr};
  raw_ptr<Observer> observer_{nullptr};

  base::WeakPtrFactory<FedCmModalDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
