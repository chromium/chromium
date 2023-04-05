// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_

#include <memory>

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

class AutocompleteResult;
class GURL;
class SessionID;
class SkBitmap;
class TemplateURL;
class TemplateURLService;
struct AutocompleteMatch;
struct OmniboxLog;

namespace bookmarks {
class BookmarkModel;
}

namespace gfx {
class Image;
struct VectorIcon;
}

class AutocompleteControllerEmitter;

using BitmapFetchedCallback =
    base::RepeatingCallback<void(int result_index, const SkBitmap& bitmap)>;
using FaviconFetchedCallback =
    base::OnceCallback<void(const gfx::Image& favicon)>;

// Interface that allows the omnibox component to interact with its embedder
// (e.g., getting information about the current page, retrieving objects
// associated with the current tab, or performing operations that rely on such
// objects under the hood).
class OmniboxClient {
 public:
  virtual ~OmniboxClient() {}

  // Returns an AutocompleteProviderClient specific to the embedder context.
  virtual std::unique_ptr<AutocompleteProviderClient>
  CreateAutocompleteProviderClient() = 0;

  // Returns whether there is any associated current page.  For example, during
  // startup or shutdown, the omnibox may exist but have no attached page.
  virtual bool CurrentPageExists() const;

  // Returns the URL of the current page.
  virtual const GURL& GetURL() const;

  // Returns the title of the current page.
  virtual const std::u16string& GetTitle() const;

  // Returns the favicon of the current page.
  virtual gfx::Image GetFavicon() const;

  // Returns whether the current page is loading.
  virtual bool IsLoading() const;

  // Returns whether paste-and-go functionality is enabled.
  virtual bool IsPasteAndGoEnabled() const;

  // Returns false if Default Search is disabled by a policy.
  virtual bool IsDefaultSearchProviderEnabled() const;

  // Returns the session ID of the current page.
  virtual SessionID GetSessionID() const = 0;

  // Called when the user changes the selected |index| in the result list via
  // mouse down or arrow key down. |match| is the suggestion corresponding to
  // that index. |navigation_predictor| represents the event indicated
  // navigation was likely.
  virtual void OnNavigationLikely(
      size_t index,
      const AutocompleteMatch& match,
      omnibox::mojom::NavigationPredictor navigation_predictor) {}

  virtual bookmarks::BookmarkModel* GetBookmarkModel();
  virtual AutocompleteControllerEmitter* GetAutocompleteControllerEmitter();
  virtual TemplateURLService* GetTemplateURLService();
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual bool ShouldDefaultTypedNavigationsToHttps() const = 0;
  // Returns the port used by the embedded https server in tests. This is used
  // to determine the correct port while upgrading typed URLs to https if the
  // original URL has a non-default port. Only meaningful if
  // ShouldDefaultTypedNavigationsToHttps() returns true.
  // TODO(crbug.com/1168371): Remove when URLLoaderInterceptor can simulate
  // redirects.
  virtual int GetHttpsPortForTesting() const = 0;

  // If true, indicates that the tests are using a faux-HTTPS server which is
  // actually an HTTP server that pretends to serve HTTPS responses. Should only
  // be true on iOS.
  virtual bool IsUsingFakeHttpsForHttpsUpgradeTesting() const = 0;

  // Returns the icon corresponding to |match| if match is an extension match
  // and an empty icon otherwise.
  virtual gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const;

  // Returns the given |vector_icon_type| with the correct size.
  virtual gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                                  SkColor vector_icon_color) const;

  // Returns the given |icon| with the correct size.
  virtual gfx::Image GetSizedIcon(const gfx::Image& icon) const;

  // Checks whether |template_url| is an extension keyword; if so, asks the
  // ExtensionOmniboxEventRouter to process |match| for it and returns true.
  // Otherwise returns false. |observer| is the OmniboxNavigationObserver
  // that was created by CreateOmniboxNavigationObserver() for |match|; in some
  // embedding contexts, processing an extension keyword involves invoking
  // action on this observer.
  virtual bool ProcessExtensionKeyword(const std::u16string& text,
                                       const TemplateURL* template_url,
                                       const AutocompleteMatch& match,
                                       WindowOpenDisposition disposition);

  // Called to notify clients that the omnibox input state has changed.
  virtual void OnInputStateChanged() {}

  // Called to notify clients that the omnibox focus state has changed.
  virtual void OnFocusChanged(OmniboxFocusState state,
                              OmniboxFocusChangeReason reason) {}

  // Called to notify the clients that the user has pasted into the omnibox, and
  // the resulting string in the omnibox is a valid URL.
  virtual void OnUserPastedInOmniboxResultingInValidURL();

  // Called when the autocomplete result has changed. Implementations that
  // support preloading (currently, prefetching or prerendering) of search
  // results pages should preload only if `should_preload` is true. If the
  // implementation supports fetching of bitmaps for URLs (not all embedders
  // do), `on_bitmap_fetched` will be called when the bitmap has been fetched.
  virtual void OnResultChanged(const AutocompleteResult& result,
                               bool default_match_changed,
                               bool should_preload,
                               const BitmapFetchedCallback& on_bitmap_fetched) {
  }

  // These two methods fetch favicons if the embedder supports it. Not all
  // embedders do. These methods return the favicon synchronously if possible.
  // Otherwise, they return an empty gfx::Image and |on_favicon_fetched| may or
  // may not be called asynchronously later. |on_favicon_fetched| will never be
  // run synchronously, and will never be run with an empty result.
  virtual gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched);
  virtual gfx::Image GetFaviconForDefaultSearchProvider(
      FaviconFetchedCallback on_favicon_fetched);
  virtual gfx::Image GetFaviconForKeywordSearchProvider(
      const TemplateURL* template_url,
      FaviconFetchedCallback on_favicon_fetched);

  // Called when the text may have changed in the edit.
  virtual void OnTextChanged(const AutocompleteMatch& current_match,
                             bool user_input_in_progress,
                             const std::u16string& user_text,
                             const AutocompleteResult& result,
                             bool has_focus) {}

  // Called when input has been accepted.
  virtual void OnInputAccepted(const AutocompleteMatch& match) {}

  // Called when the edit model is being reverted back to its unedited state.
  virtual void OnRevert() {}

  // Called to notify clients that a URL was opened from the omnibox.
  virtual void OnURLOpenedFromOmnibox(OmniboxLog* log) {}

  // Called when a bookmark is launched from the omnibox.
  virtual void OnBookmarkLaunched() {}

  // Discards the state for all pending and transient navigations.
  virtual void DiscardNonCommittedNavigations() {}

  // Presents prompt to update Chrome.
  virtual void OpenUpdateChromeDialog() {}

  // Focuses the `WebContents`, i.e. the web page of the current tab.
  virtual void FocusWebContents() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
