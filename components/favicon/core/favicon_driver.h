// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_

#include "base/observer_list.h"
#include "components/favicon/core/favicon_driver_observer.h"

class GURL;

namespace gfx {
class Image;
}

namespace favicon {

// Interface that allows favicon core code to obtain information about the
// current page. This is partially implemented by FaviconDriverImpl, and
// concrete implementation should be based on that class instead of directly
// subclassing FaviconDriver.
class FaviconDriver {
 public:
  FaviconDriver(const FaviconDriver&) = delete;
  FaviconDriver& operator=(const FaviconDriver&) = delete;

  // Adds/Removes an observer.
  void AddObserver(FaviconDriverObserver* observer);
  void RemoveObserver(FaviconDriverObserver* observer);

  // Initiates loading the favicon for the specified url. `is_same_document`
  // is true for cases where this page URL follows a navigation within the same
  // document (e.g. fragment navigation).
  virtual void FetchFavicon(const GURL& page_url, bool is_same_document) = 0;

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. Returns an empty bitmap if there are no navigation entries, which
  // should rarely happen.
  virtual gfx::Image GetFavicon() const = 0;

  // Returns true if we have the favicon for the page.
  virtual bool FaviconIsValid() const = 0;

  // Returns the URL of the current page, if any. Returns an invalid URL
  // otherwise.
  virtual GURL GetActiveURL() = 0;

 protected:
  FaviconDriver();
  virtual ~FaviconDriver();

  // Notifies FaviconDriverObservers that the favicon image has been updated.
  void NotifyFaviconUpdatedObservers(
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image);

 private:
  base::ObserverList<FaviconDriverObserver>::Unchecked observer_list_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
