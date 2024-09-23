// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "url/gurl.h"

class CommandUpdaterImpl;
class Profile;
class ReloadButton;
class LocationBarModel;

namespace views {
class WebView;
class Widget;
class WidgetDelegate;
}  // namespace views

namespace ash {

class StubBubbleModelDelegate;

// View class which shows the light version of the toolbar and the web contents.
// Light version of the toolbar includes back, forward buttons and location
// bar. Location bar is shown in read only mode, because this view is designed
// to be used for sign in to captive portal on login screen (when Browser
// isn't running).
class SimpleWebViewDialog : public views::View,
                            public LocationBarView::Delegate,
                            public ChromeLocationBarModelDelegate,
                            public CommandUpdaterDelegate,
                            public content::PageNavigator,
                            public content::WebContentsDelegate,
                            public ChromeWebModalDialogManagerDelegate,
                            public web_modal::WebContentsModalDialogHost {
  METADATA_HEADER(SimpleWebViewDialog, views::View)

 public:
  explicit SimpleWebViewDialog(Profile* profile);
  SimpleWebViewDialog(const SimpleWebViewDialog&) = delete;
  SimpleWebViewDialog& operator=(const SimpleWebViewDialog&) = delete;
  ~SimpleWebViewDialog() override;

  // Starts loading of the given url with HTTPS upgrades disabled so that
  // captive portals that allow HTTPS traffic before login can properly
  // display the login URL over HTTPS. HTTPS Upgrades will remain enabled for
  // subsequent navigations in this webview.
  void StartLoad(const GURL& url);

  // Inits view. Should be attached to a Widget before call.
  void Init();

  // Implements content::PageNavigator:
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  // Implements content::WebContentsDelegate:
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;

  // Implements LocationBarView::Delegate:
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  const LocationBarModel* GetLocationBarModel() const override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // Implements ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;

  // Implements CommandUpdaterDelegate:
  void ExecuteCommandWithDisposition(int id, WindowOpenDisposition) override;

  virtual std::unique_ptr<views::WidgetDelegate> MakeWidgetDelegate();

 private:
  void LoadImages();
  void UpdateButtons();
  void UpdateReload(bool is_loading, bool force);

  // Implements ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Implements web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<LocationBarModel> location_bar_model_;
  std::unique_ptr<CommandUpdaterImpl> command_updater_;

  // Controls
  raw_ptr<views::ImageButton> back_ = nullptr;
  raw_ptr<views::ImageButton> forward_ = nullptr;
  raw_ptr<ReloadButton> reload_ = nullptr;
  raw_ptr<LocationBarView> location_bar_ = nullptr;
  raw_ptr<views::WebView, DanglingUntriaged> web_view_ = nullptr;

  // Will own the `web_view_` until it is added as a child to the to the simple
  // web view dialog.
  std::unique_ptr<views::WebView> web_view_container_;

  std::unique_ptr<StubBubbleModelDelegate> bubble_model_delegate_;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_SIMPLE_WEB_VIEW_DIALOG_H_
