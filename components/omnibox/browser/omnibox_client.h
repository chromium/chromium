// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_

#include <memory>

#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ui/base/window_open_disposition.h"

class AutocompleteResult;
class GURL;
class SessionID;
class TemplateURL;
class TemplateURLService;
struct AutocompleteMatch;
struct OmniboxLog;

namespace bookmarks {
class BookmarkModel;
}

namespace gfx {
class Image;
}

class OmniboxControllerEmitter;

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

  // Returns an OmniboxNavigationObserver specific to the embedder context. May
  // return null if the embedder has no need to observe omnibox navigations.
  virtual std::unique_ptr<OmniboxNavigationObserver>
  CreateOmniboxNavigationObserver(const base::string16& text,
                                  const AutocompleteMatch& match,
                                  const AutocompleteMatch& alternate_nav_match);

  // Returns whether there is any associated current page.  For example, during
  // startup or shutdown, the omnibox may exist but have no attached page.
  virtual bool CurrentPageExists() const;

  // Returns the URL of the current page.
  virtual const GURL& GetURL() const;

  // Returns the title of the current page.
  virtual const base::string16& GetTitle() const;

  // Returns the favicon of the current page.
  virtual gfx::Image GetFavicon() const;

  // Returns whether the current page is loading.
  virtual bool IsLoading() const;

  // Returns whether paste-and-go functionality is enabled.
  virtual bool IsPasteAndGoEnabled() const;

  // Returns false if Default Search is disabled by a policy.
  virtual bool IsDefaultSearchProviderEnabled() const;

  // Returns the session ID of the current page.
  virtual const SessionID& GetSessionID() const = 0;

  virtual bookmarks::BookmarkModel* GetBookmarkModel();
  virtual OmniboxControllerEmitter* GetOmniboxControllerEmitter();
  virtual TemplateURLService* GetTemplateURLService();
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier();

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
  virtual bool ProcessExtensionKeyword(const TemplateURL* template_url,
                                       const AutocompleteMatch& match,
                                       WindowOpenDisposition disposition,
                                       OmniboxNavigationObserver* observer);

  // Called to notify clients that the omnibox input state has changed.
  virtual void OnInputStateChanged() {}

  // Called to notify clients that the omnibox focus state has changed.
  virtual void OnFocusChanged(OmniboxFocusState state,
                              OmniboxFocusChangeReason reason) {}

  // Called when the autocomplete result has changed. If the embedder supports
  // fetching of bitmaps for URLs (not all embedders do), |on_bitmap_fetched|
  // will be called when the bitmap has been fetched.
  virtual void OnResultChanged(const AutocompleteResult& result,
                               bool default_match_changed,
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

  // Called when the current autocomplete match has changed.
  virtual void OnCurrentMatchChanged(const AutocompleteMatch& match) {}

  // Called when the text may have changed in the edit.
  virtual void OnTextChanged(const AutocompleteMatch& current_match,
                             bool user_input_in_progress,
                             const base::string16& user_text,
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

  // Opens and shows a new incognito browser window.
  virtual void NewIncognitoWindow() {}

  // Presents translation prompt for current tab web contents.
  virtual void PromptPageTranslation() {}

  // Presents prompt to update Chrome.
  virtual void OpenUpdateChromeDialog() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
