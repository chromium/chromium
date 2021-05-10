// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/browsing_data/content/canonical_cookie_hash.h"
#include "net/cookies/cookie_monster.h"

class GURL;

namespace net {
class CanonicalCookie;
}
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
  content::StoragePartition* storage_partition_;
  IsDeletionDisabledCallback delete_disabled_callback_;

  DISALLOW_COPY_AND_ASSIGN(CookieHelper);
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

  // Adds the cookies from |details.cookie_list|. Current cookies that have the
  // same cookie name, cookie domain, cookie path, host-only-flag tuple as
  // passed cookies are replaced by the passed cookies.
  void AddCookies(const content::CookieAccessDetails& details);

  // Clears the list of canned cookies.
  void Reset();

  // True if no cookie are currently stored.
  bool empty() const;

  // CookieHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteCookie(const net::CanonicalCookie& cookie) override;

  // Returns the number of stored cookies.
  size_t GetCookieCount() const;

  // Directly returns stored cookies.
  net::CookieList GetCookieList();

  // Returns the map that contains the cookie lists for all frame urls.
  const OriginCookieSetMap& origin_cookie_set_map() {
    return origin_cookie_set_map_;
  }

 private:
  // Check if the cookie set contains a cookie with the same name,
  // domain, and path as the newly created cookie. Delete the old cookie
  // if does.
  bool DeleteMatchingCookie(const net::CanonicalCookie& add_cookie,
                            canonical_cookie::CookieHashSet* cookie_set);

  ~CannedCookieHelper() override;

  // Returns the |CookieSet| for the given |origin|.
  canonical_cookie::CookieHashSet* GetCookiesFor(const GURL& origin);

  // Adds the |cookie| to the cookie set for the given |frame_url|.
  void AddCookie(const GURL& frame_url, const net::CanonicalCookie& cookie);

  // Map that contains the cookie sets for all frame origins.
  OriginCookieSetMap origin_cookie_set_map_;

  DISALLOW_COPY_AND_ASSIGN(CannedCookieHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_COOKIE_HELPER_H_
