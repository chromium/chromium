// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_observer.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves chrome-untrusted://new-tab-page/* sources which can return content
// from outside the chromium codebase. The chrome-untrusted://new-tab-page/*
// sources can only be embedded in the chrome://new-tab-page by using an
// <iframe>.
//
// Offers the following helpers to embed content into chrome://new-tab-page in a
// generalized way:
//   * chrome-untrusted://new-tab-page/image?<url>: Behaves like an img element
//       with src set to <url>.
//   * chrome-untrusted://new-tab-page/background_image?<url>: Behaves like an
//       element that has <url> set as the background image, such that the image
//       will cover the entire element.
//   * chrome-untrusted://new-tab-page/custom_background_image?<params>: Similar
//       to background_image but allows for custom styling. <params> are of the
//       form <key>=<value>. The following keys are supported:
//         * url:       background image URL.
//         * url2x:     (optional) URL to a higher res background image.
//         * size:      (optional) CSS background-size property.
//         * repeatX:   (optional) CSS background-repeat-x property.
//         * repeatY:   (optional) CSS background-repeat-y property.
//         * positionX: (optional) CSS background-position-x property.
//         * positionY: (optional) CSS background-position-y property.
//   Each of those helpers only accept URLs with HTTPS or chrome-untrusted:.
class UntrustedSource : public content::URLDataSource,
                        public OneGoogleBarServiceObserver {
 public:
  explicit UntrustedSource(Profile* profile);
  ~UntrustedSource() override;
  UntrustedSource(const UntrustedSource&) = delete;
  UntrustedSource& operator=(const UntrustedSource&) = delete;

  // content::URLDataSource:
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching() override;
  bool ShouldReplaceExistingSource() override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

 private:
  // OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  void ServeBackgroundImage(const GURL& url,
                            const GURL& url_2x,
                            const std::string& size,
                            const std::string& repeat_x,
                            const std::string& repeat_y,
                            const std::string& position_x,
                            const std::string& position_y,
                            const std::string& scrim_display,
                            content::URLDataSource::GotDataCallback callback);
  bool IsURLBlockedByPolicy(const GURL& url);

  std::vector<content::URLDataSource::GotDataCallback>
      one_google_bar_callbacks_;
  // This dangling raw_ptr occurred in:
  // browser_tests: All/TabSharingUIViewsBrowserTest.ChangeCapturedTabFavicon/0
  // https://ci.chromium.org/ui/p/chromium-m113/builders/try/win-rel/958/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Abrowser_tests%2FTabSharingUIViewsBrowserTest.ChangeCapturedTabFavicon%2FAll.0+VHash%3Abdbee181b3e0309b
  raw_ptr<OneGoogleBarService, FlakyDanglingUntriaged> one_google_bar_service_;
  base::ScopedObservation<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observation_{this};
  std::optional<base::TimeTicks> one_google_bar_load_start_time_;
  raw_ptr<Profile, FlakyDanglingUntriaged> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
