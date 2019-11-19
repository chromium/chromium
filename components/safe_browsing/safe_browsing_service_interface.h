// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_INTERFACE_H_
#define COMPONENTS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_INTERFACE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

class SafeBrowsingServiceFactory;

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

 protected:
  SafeBrowsingServiceInterface() {}
  virtual ~SafeBrowsingServiceInterface() {}

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingServiceInterface>;

  // The factory used to instantiate a SafeBrowsingServiceInterface object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingServiceInterface.
  static SafeBrowsingServiceFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceInterface);
};

// Factory for creating SafeBrowsingServiceInterface.  Useful for tests.
class SafeBrowsingServiceFactory {
 public:
  SafeBrowsingServiceFactory() {}
  virtual ~SafeBrowsingServiceFactory() {}

  // TODO(crbug/925153): Once callers of this function are no longer downcasting
  // it to the SafeBrowsingService, we can make this a scoped_refptr.
  virtual SafeBrowsingServiceInterface* CreateSafeBrowsingService() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactory);
};

}  // namespace safe_browsing

#endif  //  COMPONENTS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_INTERFACE_H_
