// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/spoof_checks/idna_metrics.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/page_transition_types.h"
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
}  // namespace gfx

class AutocompleteControllerEmitter;
class PrefService;

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
  OmniboxClient() = default;
  virtual ~OmniboxClient() = default;

  // Returns an AutocompleteProviderClient specific to the embedder context.
  virtual std::unique_ptr<AutocompleteProviderClient>
  CreateAutocompleteProviderClient() = 0;

  // Returns whether there is any associated current page.  For example, during
  // startup or shutdown, the omnibox may exist but have no attached page.
  virtual bool CurrentPageExists() const;

  // Returns the virtual URL currently being displayed in the URL bar, if there
  // is one. This URL might be a pending navigation that hasn't committed yet,
  // so it is not guaranteed to match the current page in this WebContents.
  virtual const GURL& GetURL() const;

  // Returns the title of the current page.
  virtual const std::u16string& GetTitle() const;

  // Returns the favicon of the current page.
  virtual gfx::Image GetFavicon() const;

  // Returns the UKM source id for the top frame of the current page.
  virtual ukm::SourceId GetUKMSourceId() const;

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

  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;
  virtual bookmarks::BookmarkModel* GetBookmarkModel();
  virtual AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() = 0;
  virtual TemplateURLService* GetTemplateURLService();
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual bool ShouldDefaultTypedNavigationsToHttps() const = 0;
  // Returns the port used by the embedded https server in tests. This is used
  // to determine the correct port while upgrading typed URLs to https if the
  // original URL has a non-default port. Only meaningful if
  // ShouldDefaultTypedNavigationsToHttps() returns true.
  // TODO(crbug.com/40743298): Remove when URLLoaderInterceptor can simulate
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

  // Returns the formatted full URL for the toolbar. The formatting includes:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  // This method specifically keeps the URL suitable for editing by not
  // applying any elisions that change the meaning of the URL.
  virtual std::u16string GetFormattedFullURL() const = 0;

  // Returns a simplified URL for display (but not editing) on the toolbar.
  // This formatting is generally a superset of GetFormattedFullURL, and may
  // include some destructive elisions that change the meaning of the URL.
  // The returned string is not suitable for editing, and is for display only.
  virtual std::u16string GetURLForDisplay() const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetNavigationEntryURL() const = 0;

  // Classify the current page being viewed as, for example, the new tab
  // page or a normal web page.  Used for logging omnibox events for
  // UMA opted-in users.  Examines the user's profile to determine if the
  // current page is the user's home page.
  virtual metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const = 0;

  // Returns the security level that the toolbar should display.
  virtual security_state::SecurityLevel GetSecurityLevel() const = 0;

  // Returns the cert status of the current navigation entry.
  virtual net::CertStatus GetCertStatus() const = 0;

  // Returns the id of the icon to show to the left of the address, based on the
  // current URL.  When search term replacement is active, this returns a search
  // icon.
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;

  // Returns the LensOverlaySuggestInputs if available.
  virtual std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const;

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

  // Called when the edit model is being reverted back to its unedited state.
  virtual void OnRevert() {}

  // Called to notify clients that a URL was opened from the omnibox.
  virtual void OnURLOpenedFromOmnibox(OmniboxLog* log) {}

  // Called when a bookmark is launched from the omnibox.
  virtual void OnBookmarkLaunched() {}

  // Discards the state for all pending and transient navigations.
  virtual void DiscardNonCommittedNavigations() {}

  // Focuses the `WebContents`, i.e. the web page of the current tab.
  virtual void FocusWebContents() {}

  // Called when the user presses the thumbs down button on a suggestion.
  // Displays the Feedback form for submitting detailed feedback on why they
  // disliked the suggestion.
  virtual void ShowFeedbackPage(const std::u16string& input_text,
                                const GURL& destination_url) {}

  virtual void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) = 0;

  // Called when the view should update itself without restoring any tab state.
  virtual void OnInputInProgress(bool in_progress) {}

  // Called when the omnibox popup is shown or hidden.
  virtual void OnPopupVisibilityChanged(bool popup_is_open) {}

  // Called when the thumbnail image has been removed.
  virtual void OnThumbnailRemoved() {}

  // Even though IPH suggestions aren't selectable like normal matches, they can
  // have a 'learn more' or next-steps link. `OpenIphLink()` allows opening
  // these in a new tab.
  virtual void OpenIphLink(GURL gurl) {}

  // Returns true if history embeddings is enabled and user has opted in.
  virtual bool IsHistoryEmbeddingsEnabled() const;

  virtual base::WeakPtr<OmniboxClient> AsWeakPtr() = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
