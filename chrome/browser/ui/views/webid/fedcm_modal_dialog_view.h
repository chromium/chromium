// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog allowing the user to complete a flow (e.g. signing in to an identity
// provider) prompted by FedCM.
class FedCmModalDialogView : public views::DialogDelegateView,
                             public content::WebContentsObserver,
                             public ChromeWebModalDialogManagerDelegate {
 public:
  METADATA_HEADER(FedCmModalDialogView);

  class Observer {
   public:
    // Tells observers that their references to the view are becoming invalid.
    virtual void OnFedCmModalDialogViewDestroyed() = 0;
  };

  FedCmModalDialogView(content::WebContents* web_contents,
                       const GURL& url,
                       FedCmModalDialogView::Observer* observer);
  FedCmModalDialogView(const FedCmModalDialogView&) = delete;
  FedCmModalDialogView& operator=(const FedCmModalDialogView&) = delete;
  ~FedCmModalDialogView() override;

  // Shows a modal dialog of |url| prompted by FedCM on |web_contents|. The
  // |url| is commonly but not limited to a URL which allows the user to sign in
  // with an identity provider.
  static FedCmModalDialogView* ShowFedCmModalDialog(
      content::WebContents* web_contents,
      const GURL& url,
      FedCmModalDialogView::Observer* observer);
  void CloseFedCmModalDialog();

  content::WebContents* GetWebViewWebContents();

  void RemoveObserver();

 private:
  views::View* PopulateSheetHeaderView(views::View* container, const GURL& url);
  void Init(const GURL& url);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<views::View> contents_wrapper_;
  raw_ptr<views::WebView> web_view_;
  raw_ptr<views::Label> origin_label_;
  raw_ptr<Observer> observer_;
  url::Origin curr_origin_;

  base::WeakPtrFactory<FedCmModalDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_MODAL_DIALOG_VIEW_H_
