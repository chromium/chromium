// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_
#define CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
class V5GetHashProtocolManager;
}

// Represents a single request to the Safe Browsing service to check whether
// a website is safe when sharing files with the Web Share API. It is used for
// PDFs for instance on Desktop platforms. Must be created and used on the UI
// thread.
class SafeBrowsingRequest {
 public:
  // LINT.IfChange(WebShareSafeBrowsingCheckResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CheckResult {
    // The check completed and the URL was safe.
    kSafe = 0,
    // The check completed and the URL was unsafe (blocklisted).
    kUnsafe = 1,
    // The check timed out before a result was received.
    kTimeout = 2,
    kMaxValue = kTimeout,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:WebShareSafeBrowsingCheckResult)

  // Constructs a request that check whether a website |url| is safe by
  // consulting the |database_manager| and |v5_get_hash_protocol_manager|, and
  // invokes |callback| when done.
  //
  // It is guaranteed that |callback| will never be invoked synchronously, and
  // it will not be invoked after |this| goes out of scope.
  SafeBrowsingRequest(scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
                          database_manager,
                      base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
                          v5_get_hash_protocol_manager,
                      const GURL& url,
                      base::OnceCallback<void(bool)> callback);
  ~SafeBrowsingRequest();

 private:
  class SafeBrowsingClient;

  SafeBrowsingRequest(const SafeBrowsingRequest&) = delete;
  SafeBrowsingRequest& operator=(const SafeBrowsingRequest&) = delete;

  // Posted by the |client_| on the UI thread when it gets a response.
  void OnResultReceived(bool is_url_safe);

  // The client interfacing with Safe Browsing.
  std::unique_ptr<SafeBrowsingClient> client_;

  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<SafeBrowsingRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_
