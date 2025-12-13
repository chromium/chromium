// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_OMNIBOX_CLIENT_H_

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "ui/gfx/vector_icon_types.h"

class AutocompleteSchemeClassifier;
class Profile;

namespace content {
class WebContents;
}

// TODO(crbug.com/40263573): Consider inheriting from `ChromeOmniboxClient`
//  to avoid reimplementation of methods like `GetPrefs`.
//
// A base OmniboxClient implementation for WebUI searchboxes.
// This class provides common functionality for all WebUI searchboxes,
// but specific searchbox clients must inherit from this class to
// implement functionality specific to their use case, such as
// `GetPageClassification()`.
class SearchboxOmniboxClient : public OmniboxClient {
 public:
  SearchboxOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~SearchboxOmniboxClient() override;

  content::WebContents* web_contents() const { return web_contents_; }

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool IsPasteAndGoEnabled() const override;
  SessionID GetSessionID() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched) override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void OnAutocompleteAccept(
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
      const AutocompleteMatch& alternative_nav_match) override;
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

 protected:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

 private:
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  // This is unused, but needed for `GetVectorIcon()`.
  gfx::VectorIcon vector_icon_{nullptr, 0u, ""};
  base::WeakPtrFactory<SearchboxOmniboxClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_OMNIBOX_CLIENT_H_
