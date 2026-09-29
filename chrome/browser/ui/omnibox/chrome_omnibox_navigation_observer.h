// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/omnibox/chrome_omnibox_navigation_observer_base.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

class GURL;
class Profile;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// Desktop implementation of ChromeOmniboxNavigationObserverBase. When the
// alternative navigation probe succeeds, shows the alternate navigation
// infobar once the search result has also loaded. See
// AlternateNavInfoBarDelegate.
//
// Please see the class comment on the base class for important information
// about the memory management of this object.
class ChromeOmniboxNavigationObserver
    : public ChromeOmniboxNavigationObserverBase {
 public:
  using ShowInfobarCallback =
      base::OnceCallback<void(ChromeOmniboxNavigationObserver*)>;

  static void Create(content::NavigationHandle* navigation,
                     Profile* profile,
                     const std::u16string& text,
                     const AutocompleteMatch& match,
                     const AutocompleteMatch& alternative_nav_match);

  static void CreateForTesting(content::NavigationHandle* navigation,
                               Profile* profile,
                               const std::u16string& text,
                               const AutocompleteMatch& match,
                               const AutocompleteMatch& alternative_nav_match,
                               network::mojom::URLLoaderFactory* loader_factory,
                               ShowInfobarCallback show_infobar);

  // Shows the alternate navigation infobar for `web_contents`. Dispatches to
  // the centralized infobar framework when migrated, or falls back to
  // AlternateNavInfoBarDelegate.
  static void ShowAlternativeNavInfoBar(content::WebContents* web_contents,
                                        const std::u16string& text,
                                        const AutocompleteMatch& match,
                                        const GURL& search_url);

 private:
  ChromeOmniboxNavigationObserver(
      content::NavigationHandle& navigation,
      Profile* profile,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      network::mojom::URLLoaderFactory* loader_factory,
      ShowInfobarCallback show_infobar);

  ~ChromeOmniboxNavigationObserver() override;

  // ChromeOmniboxNavigationObserverBase:
  void OnSuccessfulNavigation() override;

  void ShowAlternativeNavInfoBar();

  // Callback to allow tests to inject custom behaviour.
  ShowInfobarCallback show_infobar_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
