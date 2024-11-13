// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SERVICE_INTERFACE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SERVICE_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/android/referring_app_info.h"
#endif

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace network::mojom {
class NetworkContext;
}

namespace safe_browsing {

class SafeBrowsingServiceFactory;
class ReferrerChainProvider;

// This interface will provide methods for checking the safety of URLs and
// downloads with Safe Browsing.
class SafeBrowsingServiceInterface
    : public base::RefCountedThreadSafe<
          SafeBrowsingServiceInterface,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  // Makes the passed |factory| the factory used to instantiate
  // a SafeBrowsingServiceInterface. Useful for tests.
  static void RegisterFactory(SafeBrowsingServiceFactory* factory) {
    factory_ = factory;
  }

  static bool HasFactory() { return (factory_ != nullptr); }

  // Create an instance of the safe browsing service.
  static SafeBrowsingServiceInterface* CreateSafeBrowsingService();

  SafeBrowsingServiceInterface(const SafeBrowsingServiceInterface&) = delete;
  SafeBrowsingServiceInterface& operator=(const SafeBrowsingServiceInterface&) =
      delete;

  virtual network::mojom::NetworkContext* GetNetworkContext(
      content::BrowserContext* browser_context) = 0;

  virtual ReferrerChainProvider* GetReferrerChainProviderFromBrowserContext(
      content::BrowserContext* browser_context) = 0;

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) = 0;
#endif

  // Report the external app redirect to Safe Browsing if the following
  // conditions are met:
  // - User is opted in to ESB and not Incognito
  // - The user has not redirected to this app recently
  // - Neither the current page nor the destination app are allowlisted.
  virtual void ReportExternalAppRedirect(content::WebContents* web_contents,
                                         std::string_view app_name,
                                         std::string_view uri) = 0;

 protected:
  SafeBrowsingServiceInterface() = default;
  virtual ~SafeBrowsingServiceInterface() = default;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingServiceInterface>;

  // The factory used to instantiate a SafeBrowsingServiceInterface object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingServiceInterface.
  static SafeBrowsingServiceFactory* factory_;
};

// Factory for creating SafeBrowsingServiceInterface.  Useful for tests.
class SafeBrowsingServiceFactory {
 public:
  SafeBrowsingServiceFactory() = default;

  SafeBrowsingServiceFactory(const SafeBrowsingServiceFactory&) = delete;
  SafeBrowsingServiceFactory& operator=(const SafeBrowsingServiceFactory&) =
      delete;

  virtual ~SafeBrowsingServiceFactory() = default;

  // TODO(crbug.com/41437292): Once callers of this function are no longer
  // downcasting it to the SafeBrowsingService, we can make this a
  // scoped_refptr.
  virtual SafeBrowsingServiceInterface* CreateSafeBrowsingService() = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_SERVICE_INTERFACE_H_
