// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#endif

class Profile;

namespace content {
class WebContents;
}  // namespace content

class NewTabPageThirdPartyHandler
    : public new_tab_page_third_party::mojom::PageHandler,
      public ThemeServiceObserver,
#if BUILDFLAG(IS_ANDROID)
      public content::WebContentsObserver,
#endif
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

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/571788628): Unlike Desktop, Android's ColorProvider is not
  // owned by the WebContents. It is instead owned by the Activity, which gets
  // recreated during a theme switch. By observing the ColorProvider, we can
  // ensure the theme is updated when the Activity is recreated.
  void OnColorProviderChanged() override;
#endif
  void NotifyAboutTheme();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/571788628): On Android a light/dark switch is an Activity
  // theme change. ThemeService only notifies when the color scheme is changed
  // through it, not when it is changed in Settings or follows the system,
  // whereas the NativeTheme is updated for every switch, so observe it like the
  // first-party NewTabPageHandler does.
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
#endif

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<new_tab_page_third_party::mojom::Page> page_;
  mojo::Receiver<new_tab_page_third_party::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<NewTabPageThirdPartyHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HANDLER_H_
