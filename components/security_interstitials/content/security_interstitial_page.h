// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}

namespace security_interstitials {
class SecurityInterstitialControllerClient;

// Represents a single interstitial, which is associated to either a subframe or
// main frame.
// TODO(crbug.com/369755672): Rename to SecurityInterstitialDocument.
class SecurityInterstitialPage {
 public:
  // An identifier used to identify a SecurityInterstitialPage.
  typedef const void* TypeID;

  // |request_url| is the URL which triggered the interstitial document.
  SecurityInterstitialPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<SecurityInterstitialControllerClient> controller);

  SecurityInterstitialPage(const SecurityInterstitialPage&) = delete;
  SecurityInterstitialPage& operator=(const SecurityInterstitialPage&) = delete;

  virtual ~SecurityInterstitialPage();

  // Called when the interstitial is committed.
  void OnInterstitialShown();

  // Prevents creating the actual interstitial view for testing.
  void DontCreateViewForTesting();

  // Returns the HTML for the error page.
  virtual std::string GetHTMLContents();

  // Must be called when the interstitial is closed, to give subclasses a chance
  // to e.g. update metrics.
  virtual void OnInterstitialClosing() = 0;

  // Whether a URL should be displayed on this interstitial page. This is
  // respected by committed interstitials only.
  virtual bool ShouldDisplayURL() const;

  // Invoked when the user interacts with the interstitial.
  virtual void CommandReceived(const std::string& command) {}

  // If `this` was created for a post commit error page,
  // `error_page_navigation_handle` is the navigation created for this blocking
  // page.
  virtual void CreatedPostCommitErrorPageNavigation(
      content::NavigationHandle* error_page_navigation_handle) {}

  // Return the interstitial type for testing.
  virtual TypeID GetTypeForTesting();

 protected:
  // Populates the strings used to generate the HTML from the template.
  virtual void PopulateInterstitialStrings(
      base::Value::Dict& load_time_data) = 0;

  virtual int GetHTMLTemplateId();

  // Returns the formatted host name for the request url.
  std::u16string GetFormattedHostName() const;

  content::WebContents* web_contents() const;
  GURL request_url() const;

  SecurityInterstitialControllerClient* controller() const;

  // Update metrics when the interstitial is closed.
  void UpdateMetricsAfterSecurityInterstitial();

 private:
  void SetUpMetrics();

  // The WebContents with which this interstitial page is
  // associated. Not available in ~SecurityInterstitialPage, since it
  // can be destroyed before this class is destroyed.
  raw_ptr<content::WebContents> web_contents_;
  const GURL request_url_;
  // Whether the interstitial should create a view.
  bool create_view_;

  // Store some data about the initial state of extended reporting opt-in.
  bool on_show_extended_reporting_pref_value_;

  // For subclasses that don't have their own ControllerClients yet.
  std::unique_ptr<SecurityInterstitialControllerClient> controller_;
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_
