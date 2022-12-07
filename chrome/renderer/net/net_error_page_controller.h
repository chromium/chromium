// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
#define CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"

namespace content {
class RenderFrame;
}

// This class makes various helper functions available to the
// error page loaded by NetErrorHelper.  It is bound to the JavaScript
// window.errorPageController object.
class NetErrorPageController : public gin::Wrappable<NetErrorPageController> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Interface used to notify creator of user actions invoked on the error page.
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Button press notification from error page.
    virtual void ButtonPressed(NetErrorHelperCore::Button button) = 0;

    // Called to open suggested offline content when it is pressed.
    virtual void LaunchOfflineItem(const std::string& id,
                                   const std::string& name_space) = 0;

    // Called to show all available offline content.
    virtual void LaunchDownloadsPage() = 0;

    // Schedules a request to save the page later. This is different from the
    // download button in that the page is only saved temporarily. This is used
    // only for the auto-fetch-on-net-error-page feature.
    virtual void SavePageForLater() = 0;

    // Cancels the request to save the page later. This cancels a previous call
    // to |SavePageForLater|, or the automatic request made when loading the
    // error page. This is used only for the auto-fetch-on-net-error-page
    // feature.
    virtual void CancelSavePage() = 0;

    // Called to signal the user tapped the button to change the visibility of
    // the offline content list.
    virtual void ListVisibilityChanged(bool is_visible) = 0;

    // Save a new high score for the easer egg game in the user's synced
    // preferences.
    virtual void UpdateEasterEggHighScore(int high_score) = 0;

    // Clear any high score for the easer egg game saved in the user's synced
    // preferences.
    virtual void ResetEasterEggHighScore() = 0;

   protected:
    Delegate();
    virtual ~Delegate();
  };

  NetErrorPageController(const NetErrorPageController&) = delete;
  NetErrorPageController& operator=(const NetErrorPageController&) = delete;

  // Will invoke methods on |delegate| in response to user actions taken on the
  // error page. May call delegate methods even after the page has been
  // navigated away from, so it is recommended consumers make sure the weak
  // pointers are destroyed in response to navigations.
  static void Install(content::RenderFrame* render_frame,
                      base::WeakPtr<Delegate> delegate);

 private:
  explicit NetErrorPageController(base::WeakPtr<Delegate> delegate);
  ~NetErrorPageController() override;

  void ErrorPageLoadedOrUpdated();

  // Execute a button click to download page later.
  bool DownloadButtonClick();

  // Execute a "Reload" button click.
  bool ReloadButtonClick();

  // Execute a "Details" button click.
  bool DetailsButtonClick();

  // Track easter egg plays and high scores.
  bool TrackEasterEgg();
  bool UpdateEasterEggHighScore(int high_score);
  bool ResetEasterEggHighScore();

  // Execute a "Diagnose Errors" button click.
  bool DiagnoseErrorsButtonClick();

  // Execute a "Sign in to network" button click.
  bool PortalSigninButtonClick();

  // Used internally by other button click methods.
  bool ButtonClick(NetErrorHelperCore::Button button);

  void LaunchOfflineItem(gin::Arguments* args);
  void LaunchDownloadsPage();
  void SavePageForLater();
  void CancelSavePage();
  void ListVisibilityChanged(bool is_visible);

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  base::WeakPtr<Delegate> delegate_;
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
