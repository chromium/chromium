// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Browser;
class Profile;

namespace views {
class WebDialogView;
class Widget;
}

// Happiness tracking survey dialog which shows the survey content.
// This class lives on the UI thread and is self deleting.
// TODO(weili): This dialog shares a lot of common code with the one in
// chrome/browser/chromeos/hats/, should be merged into one.
class HatsWebDialog : public ui::WebDialogDelegate,
                      public views::DialogDelegateView,
                      public ProfileObserver {
 public:
  // Create an instance of HatsWebDialog and load its content without showing.
  static void Create(Browser* browser, const std::string& site_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(HatsWebDialogBrowserTest, Cookies);
  friend class TestHatsWebDialog;
  friend class HatsWebDialogBrowserTest;

  // Use Create() above. An off the record profile is created from the given
  // browser profile which is used for navigating to the survey. |site_id| is
  // used to select the survey.
  HatsWebDialog(Browser* browser, const std::string& site_id);
  ~HatsWebDialog() override;

  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  void OnWebContentsFinishedLoad() override;
  void OnMainFrameResourceLoadComplete(
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  // views::DialogDelegateView implementation.
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  ui::ModalType GetModalType() const override;

  // These are virtual for tests.
  virtual void OnLoadTimedOut();
  virtual const base::TimeDelta ContentLoadingTimeout() const;

  Profile* otr_profile_for_testing() { return otr_profile_; }

  void CreateWebDialog(Browser* browser);
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;
  void Show(views::Widget* widget, bool accept);

  Profile* otr_profile_;
  Browser* browser_;
  const std::string site_id_;

  // A timer to prevent unresponsive loading of survey dialog.
  base::OneShotTimer loading_timer_;

  // The widget created for preloading. It is owned by us until it is shown to
  // user.
  views::Widget* preloading_widget_ = nullptr;
  views::WebDialogView* webview_ = nullptr;

  // Indicate whether HaTS resources were loaded successfully.
  bool resource_loaded_ = false;

  base::WeakPtrFactory<HatsWebDialog> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HatsWebDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
