// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cookie_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace browsing_data {
namespace {
const char kGlobalCookieSetURL[] = "chrome://cookieset";
}  // namespace

CookieHelper::CookieHelper(content::StoragePartition* storage_partition,
                           IsDeletionDisabledCallback callback)
    : storage_partition_(storage_partition),
      delete_disabled_callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CookieHelper::~CookieHelper() {}

void CookieHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  storage_partition_->GetCookieManagerForBrowserProcess()->GetAllCookies(
      std::move(callback));
}

void CookieHelper::DeleteCookie(const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delete_disabled_callback_ &&
      delete_disabled_callback_.Run(net::cookie_util::CookieOriginToURL(
          cookie.Domain(), cookie.IsSecure()))) {
    return;
  }
  storage_partition_->GetCookieManagerForBrowserProcess()
      ->DeleteCanonicalCookie(cookie, base::DoNothing());
}

CannedCookieHelper::CannedCookieHelper(
    content::StoragePartition* storage_partition,
    IsDeletionDisabledCallback callback)
    : CookieHelper(storage_partition, callback) {}

CannedCookieHelper::~CannedCookieHelper() {
  Reset();
}

void CannedCookieHelper::AddCookies(
    const content::CookieAccessDetails& details) {
  for (const auto& add_cookie : details.cookie_list)
    AddCookie(details.url, add_cookie);
}

void CannedCookieHelper::Reset() {
  origin_cookie_set_map_.clear();
}

bool CannedCookieHelper::empty() const {
  for (const auto& pair : origin_cookie_set_map_) {
    if (!pair.second->empty())
      return false;
  }
  return true;
}

size_t CannedCookieHelper::GetCookieCount() const {
  size_t count = 0;
  for (const auto& pair : origin_cookie_set_map_)
    count += pair.second->size();
  return count;
}

void CannedCookieHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(GetCookieList());
}

void CannedCookieHelper::DeleteCookie(const net::CanonicalCookie& cookie) {
  for (const auto& pair : origin_cookie_set_map_)
    DeleteMatchingCookie(cookie, pair.second.get());
  CookieHelper::DeleteCookie(cookie);
}

net::CookieList CannedCookieHelper::GetCookieList() {
  net::CookieList cookie_list;
  for (const auto& pair : origin_cookie_set_map_) {
    cookie_list.insert(cookie_list.begin(), pair.second->begin(),
                       pair.second->end());
  }
  return cookie_list;
}

bool CannedCookieHelper::DeleteMatchingCookie(
    const net::CanonicalCookie& add_cookie,
    canonical_cookie::CookieHashSet* cookie_set) {
  return cookie_set->erase(add_cookie) > 0;
}

canonical_cookie::CookieHashSet* CannedCookieHelper::GetCookiesFor(
    const GURL& first_party_origin) {
  std::unique_ptr<canonical_cookie::CookieHashSet>& entry =
      origin_cookie_set_map_[first_party_origin];
  if (entry)
    return entry.get();

  entry = std::make_unique<canonical_cookie::CookieHashSet>();
  return entry.get();
}

void CannedCookieHelper::AddCookie(const GURL& frame_url,
                                   const net::CanonicalCookie& cookie) {
  // Storing cookies in separate cookie sets per frame origin makes the
  // GetCookieCount method count a cookie multiple times if it is stored in
  // multiple sets.
  // E.g. let "example.com" be redirected to "www.example.com". A cookie set
  // with the cookie string "A=B; Domain=.example.com" would be sent to both
  // hosts. This means it would be stored in the separate cookie sets for both
  // hosts ("example.com", "www.example.com"). The method GetCookieCount would
  // count this cookie twice. To prevent this, we us a single global cookie
  // set as a work-around to store all added cookies. Per frame URL cookie
  // sets are currently not used. In the future they will be used for
  // collecting cookies per origin in redirect chains.
  // TODO(markusheintz): A) Change the GetCookiesCount method to prevent
  // counting cookies multiple times if they are stored in multiple cookie
  // sets.  B) Replace the GetCookieFor method call below with:
  // "GetCookiesFor(frame_url.GetOrigin());"
  static const base::NoDestructor<GURL> origin_cookie_url(kGlobalCookieSetURL);
  canonical_cookie::CookieHashSet* cookie_set =
      GetCookiesFor(*origin_cookie_url);
  DeleteMatchingCookie(cookie, cookie_set);
  cookie_set->insert(cookie);
}

}  // namespace browsing_data
