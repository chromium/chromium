// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_

#include "components/omnibox/browser/location_bar_model_delegate.h"

class Profile;

namespace content {
class NavigationEntry;
class NavigationController;
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Implementation of LocationBarModelDelegate for the Chrome embedder. It leaves
// out how to fetch the active WebContents to its subclasses.
class ChromeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  ChromeLocationBarModelDelegate(const ChromeLocationBarModelDelegate&) =
      delete;
  ChromeLocationBarModelDelegate& operator=(
      const ChromeLocationBarModelDelegate&) = delete;

  // Returns active WebContents.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // LocationBarModelDelegate:
  std::u16string FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::u16string& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldPreventElision() override;
  bool ShouldDisplayURL() const override;
  bool ShouldUseUpdatedConnectionSecurityIndicators() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;
  const gfx::VectorIcon* GetVectorIconOverride() const override;
  bool IsOfflinePage() const override;
  bool IsNewTabPage() const override;
  bool IsNewTabPageURL(const GURL& url) const override;
  bool IsHomePage(const GURL& url) const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  TemplateURLService* GetTemplateURLService() override;

  // Registers a preference used to prevent URL elisions.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  ChromeLocationBarModelDelegate();
  ~ChromeLocationBarModelDelegate() override;

  // Helper method to get the navigation entry from the navigation controller.
  content::NavigationEntry* GetNavigationEntry() const;

 private:
  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved. If this returns null, default values
  // are used.
  content::NavigationController* GetNavigationController() const;

  // Helper method to extract the profile from the navigation controller.
  Profile* GetProfile() const;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_
