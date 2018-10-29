// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/new_window.mojom.h"
#include "base/macros.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Handles opening new tabs and windows on behalf of ash (over mojo) and the
// ARC bridge (via a delegate in the browser process).
class ChromeNewWindowClient : public ash::mojom::NewWindowClient,
                              public arc::OpenUrlDelegate {
 public:
  ChromeNewWindowClient();
  ~ChromeNewWindowClient() override;

  static ChromeNewWindowClient* Get();

  // Overridden from ash::mojom::NewWindowClient:
  void NewTab() override;
  void NewTabWithUrl(const GURL& url, bool from_user_interaction) override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardShortcutViewer() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage() override;

  // arc::OpenUrlDelegate:
  void OpenUrlFromArc(const GURL& url) override;
  void OpenWebAppFromArc(const GURL& url) override;

 private:
  class TabRestoreHelper;

  // Opens a URL in a new tab. Returns the WebContents for the tab that
  // opened the URL. If the URL is for a chrome://settings page, opens settings
  // in a new window and returns null. If the |from_user_interaction| is true
  // then the page will load with a user activation. This means it will be able
  // to autoplay media without restriction.
  content::WebContents* OpenUrlImpl(const GURL& url,
                                    bool from_user_interaction);

  std::unique_ptr<TabRestoreHelper> tab_restore_helper_;

  ash::mojom::NewWindowControllerPtr new_window_controller_;

  // Binds this object to the client interface.
  mojo::AssociatedBinding<ash::mojom::NewWindowClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNewWindowClient);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_CLIENT_H_
