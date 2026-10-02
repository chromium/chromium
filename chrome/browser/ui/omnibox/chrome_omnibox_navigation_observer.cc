// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

ChromeOmniboxNavigationObserver::ChromeOmniboxNavigationObserver(
    content::NavigationHandle& navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory,
    ShowInfobarCallback show_infobar)
    : ChromeOmniboxNavigationObserverBase(navigation,
                                          profile,
                                          text,
                                          match,
                                          alternative_nav_match,
                                          loader_factory),
      show_infobar_(std::move(show_infobar)) {}

ChromeOmniboxNavigationObserver::~ChromeOmniboxNavigationObserver() {
  // Both the navigation and the alternative fetch (if any) hold a reference,
  // so both have finished by the time this runs. `show_infobar_` runs
  // synchronously and must not retain `this`.
  if (!web_contents()) {
    return;
  }
  if (fetch_state() == AlternativeFetchState::kFetchSucceeded) {
    std::move(show_infobar_).Run(this);
  }
}

// static
void ChromeOmniboxNavigationObserver::ShowAlternativeNavInfoBar(
    content::WebContents* web_contents,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const GURL& search_url) {
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE)) {
    // The omnibox can also drive navigations in WebContents that are not
    // tabs (e.g. the DevTools window), which have no TabInterface and thus
    // cannot host a centralized infobar.
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (!tab) {
      return;
    }
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    CHECK(browser_infobar_manager);
    infobars::InfoBarShowParams params;
    params.substitutions = {
        MessageSubstitution(base::UTF8ToUTF16(match.destination_url.spec()),
                            /*is_link=*/true, std::nullopt)};
    params.inline_link_callback = base::BindRepeating(
        [](const std::u16string& text, const AutocompleteMatch& match,
           const GURL& search_url, content::WebContents* contents,
           size_t /*index*/, WindowOpenDisposition disposition) {
          if (!contents) {
            return false;
          }
          Profile* profile =
              Profile::FromBrowserContext(contents->GetBrowserContext());
          history::HistoryService* const history_service =
              HistoryServiceFactory::GetForProfile(
                  profile, ServiceAccessType::IMPLICIT_ACCESS);
          scoped_refptr<ShortcutsBackend> shortcuts_backend(
              ShortcutsBackendFactory::GetForProfile(profile));
          if (shortcuts_backend) {
            shortcuts_backend->DeleteShortcutsWithURL(search_url);
            shortcuts_backend->AddOrUpdateShortcut(text, match);
          }
          if (history_service) {
            history_service->DeleteKeywordSearchTermForURL(search_url);
          }
          contents->OpenURL(
              content::OpenURLParams(match.destination_url, content::Referrer(),
                                     disposition, ui::PAGE_TRANSITION_TYPED,
                                     /*is_renderer_initiated=*/false),
              /*navigation_handle_callback=*/{});
          return true;
        },
        text, match, search_url);
    browser_infobar_manager->Show(
        tab, infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
        std::move(params));
    return;
  }

  AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(web_contents, text,
                                                          match, search_url);
}

void ChromeOmniboxNavigationObserver::ShowAlternativeNavInfoBar() {
  ShowAlternativeNavInfoBar(web_contents(), text(), alternative_nav_match(),
                            match().destination_url);
}

// static
void ChromeOmniboxNavigationObserver::Create(
    content::NavigationHandle* navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  TRACE_EVENT("omnibox", "ChromeOmniboxNavigationObserver::Create",
              "navigation", navigation, "match", match, "alternative_nav_match",
              alternative_nav_match);

  if (!navigation) {
    return;
  }

  // The observer will be kept alive until both navigation and the loading
  // fetcher finish.
  new ChromeOmniboxNavigationObserver(
      *navigation, profile, text, match, alternative_nav_match, nullptr,
      base::BindOnce([](ChromeOmniboxNavigationObserver* observer) {
        observer->ShowAlternativeNavInfoBar();
      }));
}

// static
void ChromeOmniboxNavigationObserver::CreateForTesting(
    content::NavigationHandle* navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory,
    ShowInfobarCallback show_infobar) {
  if (!navigation) {
    return;
  }

  // The observer will be kept alive until both navigation and the loading
  // fetcher finish.
  new ChromeOmniboxNavigationObserver(*navigation, profile, text, match,
                                      alternative_nav_match, loader_factory,
                                      std::move(show_infobar));
}
