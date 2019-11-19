// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/presentation/independent_otr_profile_manager.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Browser;
class Profile;

namespace views {
class Widget;
}

// Happiness tracking survey dialog which shows the survey content.
// This class lives on the UI thread and is self deleting.
// TODO(weili): This dialog shares a lot of common code with the one in
// chrome/browser/chromeos/hats/, should be merged into one.
class HatsWebDialog : public ui::WebDialogDelegate {
 public:
  // Create an instance of HatsWebDialog and load its content without showing.
  static void Create(Browser* browser, const std::string& site_id);

 private:
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
  bool CanResizeDialog() const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  void OnWebContentsFinishedLoad() override;
  void OnMainFrameResourceLoadComplete(
      const content::mojom::ResourceLoadInfo& resource_load_info) override;

  // These are virtual for tests.
  virtual void OnLoadTimedOut();
  virtual const base::TimeDelta ContentLoadingTimeout() const;

  Profile* off_the_record_profile() {
    return otr_profile_registration_->profile();
  }

  void CreateWebDialog(Browser* browser);
  void OnOriginalProfileDestroyed(Profile* profile);
  void Show(views::Widget* widget, bool accept);

  std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>
      otr_profile_registration_;
  Browser* browser_;
  const std::string site_id_;

  // A timer to prevent unresponsive loading of survey dialog.
  base::OneShotTimer loading_timer_;

  // The widget created for preloading. It is owned by us until it is shown to
  // user.
  views::Widget* preloading_widget_{nullptr};

  // Indicate whether HaTS resources were loaded successfully.
  bool resource_loaded_{false};

  base::WeakPtrFactory<HatsWebDialog> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HatsWebDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
