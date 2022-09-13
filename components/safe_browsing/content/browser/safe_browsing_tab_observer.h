// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_TAB_OBSERVER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_TAB_OBSERVER_H_

#include <memory>

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class ClientSideDetectionHost;
class ClientSideDetectionService;

// Per-tab class to handle safe-browsing functionality.
class SafeBrowsingTabObserver
    : public content::WebContentsUserData<SafeBrowsingTabObserver> {
 public:
  // Interface via which the embedder supplies contextual information to
  // SafeBrowsingTabObserver.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Returns the PrefService that the embedder associates with
    // |browser_context|.
    virtual PrefService* GetPrefs(content::BrowserContext* browser_context) = 0;

    // Returns the ClientSideDetectionService that the embedder associates with
    // |browser_context|, if one exists. May return null.
    virtual ClientSideDetectionService* GetClientSideDetectionServiceIfExists(
        content::BrowserContext* browser_context) = 0;

    // Returns true if the embedder-specific safe browsing service exists.
    virtual bool DoesSafeBrowsingServiceExist() = 0;

    // Creates a ClientSideDetectionHost instance that has been configured for
    // the embedder.
    virtual std::unique_ptr<ClientSideDetectionHost>
    CreateClientSideDetectionHost(content::WebContents* web_contents) = 0;
  };

  SafeBrowsingTabObserver(const SafeBrowsingTabObserver&) = delete;
  SafeBrowsingTabObserver& operator=(const SafeBrowsingTabObserver&) = delete;

  ~SafeBrowsingTabObserver() override;

 private:
  SafeBrowsingTabObserver(content::WebContents* web_contents,
                          std::unique_ptr<Delegate> delegate);
  friend class content::WebContentsUserData<SafeBrowsingTabObserver>;

  // Internal helpers ----------------------------------------------------------

  // Create or destroy SafebrowsingDetectionHost as needed if the user's
  // safe browsing preference has changed.
  void UpdateSafebrowsingDetectionHost();

  // Handles IPCs.
  std::unique_ptr<ClientSideDetectionHost> safebrowsing_detection_host_;

  std::unique_ptr<Delegate> delegate_;

  PrefChangeRegistrar pref_change_registrar_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_TAB_OBSERVER_H_
