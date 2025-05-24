// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cookie_helper.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace browsing_data {

CookieHelper::CookieHelper(content::StoragePartition* storage_partition,
                           IsDeletionDisabledCallback callback)
    : storage_partition_(storage_partition),
      delete_disabled_callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CookieHelper::~CookieHelper() = default;

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
          cookie.Domain(), cookie.SecureAttribute()))) {
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
  for (const auto& cookie_with_access_result :
       details.cookie_access_result_list) {
    const auto& cookie = cookie_with_access_result.cookie;
    auto existing_slot = origin_cookie_set_.find(cookie);
    if (existing_slot != origin_cookie_set_.end()) {
      // |cookie| already exists in the set. We need to remove the old instance
      // and add this new one. Perform the equivalent of deleting
      // the old instance from the set and inserting the new one by copying
      // |cookie| into the instance it would replace in the set.
      // Note that the set is keyed off of cookie name, domain, and path, but
      // those properties are the same between the old and new cookies (which
      // is the reason they matched in the first place). Other cookies
      // properties are the only ones that may change.
      const_cast<net::CanonicalCookie&>(*existing_slot) = cookie;
    } else {
      origin_cookie_set_.insert(cookie);
    }
  }
}

void CannedCookieHelper::Reset() {
  origin_cookie_set_.clear();
}

bool CannedCookieHelper::empty() const {
  return origin_cookie_set_.empty();
}

size_t CannedCookieHelper::GetCookieCount() const {
  return origin_cookie_set_.size();
}

void CannedCookieHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(GetCookieList());
}

void CannedCookieHelper::DeleteCookie(const net::CanonicalCookie& cookie) {
  origin_cookie_set_.erase(cookie);
  CookieHelper::DeleteCookie(cookie);
}

net::CookieList CannedCookieHelper::GetCookieList() {
  return net::CookieList(origin_cookie_set_.begin(), origin_cookie_set_.end());
}

}  // namespace browsing_data
