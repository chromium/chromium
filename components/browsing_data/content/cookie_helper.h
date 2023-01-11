// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/browsing_data/content/canonical_cookie_hash.h"
#include "net/cookies/canonical_cookie.h"

class GURL;

namespace content {
struct CookieAccessDetails;
class StoragePartition;
}

namespace browsing_data {

// This class fetches cookie information on behalf of a caller
// on the UI thread.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class CookieHelper : public base::RefCountedThreadSafe<CookieHelper> {
 public:
  using FetchCallback = base::OnceCallback<void(const net::CookieList&)>;
  using IsDeletionDisabledCallback = base::RepeatingCallback<bool(const GURL&)>;
  explicit CookieHelper(content::StoragePartition* storage_partition,
                        IsDeletionDisabledCallback callback);
  CookieHelper(const CookieHelper&) = delete;
  CookieHelper& operator=(const CookieHelper&) = delete;

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Requests a single cookie to be deleted in the IO thread. This must be
  // called in the UI thread.
  virtual void DeleteCookie(const net::CanonicalCookie& cookie);

 protected:
  friend class base::RefCountedThreadSafe<CookieHelper>;
  virtual ~CookieHelper();

 private:
  raw_ptr<content::StoragePartition> storage_partition_;
  IsDeletionDisabledCallback delete_disabled_callback_;
};

// This class is a thin wrapper around CookieHelper that does not
// fetch its information from the persistent cookie store. It is a simple
// container for CanonicalCookies. Clients that use this container can add
// cookies that are sent to a server via the AddReadCookies method and cookies
// that are received from a server or set via JavaScript using the method
// AddChangedCookie.
// Cookies are distinguished by the tuple cookie name (called cookie-name in
// RFC 6265), cookie domain (called cookie-domain in RFC 6265), cookie path
// (called cookie-path in RFC 6265) and host-only-flag (see RFC 6265 section
// 5.3). Cookies with same tuple (cookie-name, cookie-domain, cookie-path,
// host-only-flag) as cookie that are already stored, will replace the stored
// cookies.
class CannedCookieHelper : public CookieHelper {
 public:
  typedef std::map<GURL, std::unique_ptr<canonical_cookie::CookieHashSet>>
      OriginCookieSetMap;

  explicit CannedCookieHelper(content::StoragePartition* storage_partition,
                              IsDeletionDisabledCallback callback);
  CannedCookieHelper(const CannedCookieHelper&) = delete;
  CannedCookieHelper& operator=(const CannedCookieHelper&) = delete;

  // Adds the cookies from |details.cookie_list|. Current cookies that have the
  // same cookie name, cookie domain, cookie path, host-only-flag tuple as
  // passed cookies are replaced by the passed cookies.
  void AddCookies(const content::CookieAccessDetails& details);

  // Clears the list of canned cookies.
  void Reset();

  // True if no cookies are currently stored.
  bool empty() const;

  // CookieHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteCookie(const net::CanonicalCookie& cookie) override;

  // Returns the number of stored cookies.
  size_t GetCookieCount() const;

  // Directly returns stored cookies.
  net::CookieList GetCookieList();

  // Returns the set of all cookies.
  const canonical_cookie::CookieHashSet& origin_cookie_set() {
    return origin_cookie_set_;
  }

 private:
  ~CannedCookieHelper() override;

  // The cookie set for all frame origins.
  canonical_cookie::CookieHashSet origin_cookie_set_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_
