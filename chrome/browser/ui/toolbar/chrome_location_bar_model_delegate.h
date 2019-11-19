// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_

#include "base/macros.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"

class Profile;

namespace content {
class NavigationEntry;
class NavigationController;
class WebContents;
}  // namespace content

// Implementation of LocationBarModelDelegate for the Chrome embedder. It leaves
// out how to fetch the active WebContents to its subclasses.
class ChromeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  // Returns active WebContents.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // Prevents URL elision depending on whether a specified extension installed.
  bool ShouldPreventElision() const override;

  // LocationBarModelDelegate:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldDisplayURL() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;
  const gfx::VectorIcon* GetVectorIconOverride() const override;
  bool IsOfflinePage() const override;
  bool IsInstantNTP() const override;
  bool IsNewTabPage(const GURL& url) const override;
  bool IsHomePage(const GURL& url) const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  TemplateURLService* GetTemplateURLService() override;

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

  DISALLOW_COPY_AND_ASSIGN(ChromeLocationBarModelDelegate);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_LOCATION_BAR_MODEL_DELEGATE_H_
