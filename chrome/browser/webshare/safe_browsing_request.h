// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_
#define CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

// Represents a single request to the Safe Browsing service to check whether
// a website is safe when sharing files with the Web Share API. It is used for
// PDFs for instance on Desktop platforms. Can be created and used on any one
// thread.
class SafeBrowsingRequest {
 public:
  // Constructs a request that check whether a website |url| is safe by
  // consulting the |database_manager|, and invokes |callback| when done.
  //
  // It is guaranteed that |callback| will never be invoked synchronously, and
  // it will not be invoked after |this| goes out of scope.
  SafeBrowsingRequest(scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
                          database_manager,
                      const GURL& url,
                      base::OnceCallback<void(bool)> callback);
  ~SafeBrowsingRequest();

 private:
  class SafeBrowsingClient;

  SafeBrowsingRequest(const SafeBrowsingRequest&) = delete;
  SafeBrowsingRequest& operator=(const SafeBrowsingRequest&) = delete;

  // Posted by the |client_| from the IO thread when it gets a response.
  void OnResultReceived(bool is_url_safe);

  // The client interfacing with Safe Browsing.
  std::unique_ptr<SafeBrowsingClient> client_;

  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<SafeBrowsingRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_WEBSHARE_SAFE_BROWSING_REQUEST_H_
