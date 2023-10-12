// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class Profile;

// Monitors omnibox navigations in order to trigger behaviors that depend on
// successful navigations.
//
// Currently three such behaviors exist:
// (1) For single-word queries where we can't tell if the entry was a search or
//     an intranet hostname, the omnibox opens as a search by default, but this
//     class attempts to open as a URL via an HTTP HEAD request.  If successful,
//     displays an infobar once the search result has also loaded.  See
//     AlternateNavInfoBarDelegate.
// (2) Omnibox navigations that complete successfully are added to the
//     Shortcuts backend.
// (3) Omnibox searches that result in a 404 for an auto-generated custom
//     search engine cause the custom search engine to be deleted.
//
// Please see the class comment on the base class for important information
// about the memory management of this object.
class ChromeOmniboxNavigationObserver
    : public base::RefCounted<ChromeOmniboxNavigationObserver>,
      public content::WebContentsObserver {
 public:
  enum class AlternativeFetchState {
    kFetchNotComplete,
    kFetchSucceeded,
    kFetchFailed,
  };

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

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void On404();

  void OnAlternativeLoaderDone(bool success);

  void CreateAlternativeNavInfoBar();

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

  friend class base::RefCounted<ChromeOmniboxNavigationObserver>;

  class AlternativeNavigationURLLoader;

  void ShowAlternativeNavInfoBar();

  const std::u16string text_;
  const AutocompleteMatch match_;
  const AutocompleteMatch alternative_nav_match_;
  const int64_t navigation_id_;
  const raw_ptr<Profile> profile_;

  // Callback to allow tests to inject custom behaviour.
  ShowInfobarCallback show_infobar_;

  // URLLoader responsible for fetching the alternative match and showing the
  // infobar if it succeeds.
  std::unique_ptr<AlternativeNavigationURLLoader> loader_;
  AlternativeFetchState fetch_state_ = AlternativeFetchState::kFetchNotComplete;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
