// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class NewTabPageThirdPartyHandler
    : public new_tab_page_third_party::mojom::PageHandler,
      public ThemeServiceObserver,
      public ui::NativeThemeObserver {
 public:
  NewTabPageThirdPartyHandler(
      mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents);

  NewTabPageThirdPartyHandler(const NewTabPageThirdPartyHandler&) = delete;
  NewTabPageThirdPartyHandler& operator=(const NewTabPageThirdPartyHandler&) =
      delete;

  ~NewTabPageThirdPartyHandler() override;

  // new_tab_page_third_party::mojom::PageHandler:
  void UpdateTheme() override;

 private:
  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  void NotifyAboutTheme();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page_third_party::mojom::Page> page_;
  mojo::Receiver<new_tab_page_third_party::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageThirdPartyHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
