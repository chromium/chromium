// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/page_manifest_manager.h"

namespace content {
class WebContents;
}

namespace webapps {
class WebAppUrlLoader;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppIconDownloader;
class FakeWebContentsManager;

// This manager is intended to wrap all of the functionality that the
// `WebAppProvider` system needs from `WebContents`. This encompasses retrieving
// any information from a given `WebContents`.
//
// Since `WebContents` are generally not functional in unit tests, this class is
// faked by the the `FakeWebContentsManager` to allow easy unit testing.
//
// TODO(b/280517254): Have this class more fully encompass the WebContents
// dependency, instead of creating classes that operate on it.
class WebContentsManager {
 public:
  WebContentsManager();
  virtual ~WebContentsManager();

  // Creates a `WebAppUrlLoader` to load URLs in a `WebContents`.
  virtual std::unique_ptr<webapps::WebAppUrlLoader> CreateUrlLoader();

  // Creates a `WebAppDataRetriever` to retrieve information about a web app
  // from a `WebContents`.
  virtual std::unique_ptr<WebAppDataRetriever> CreateDataRetriever();

  // Creates a `WebAppIconDownloader` to download icons for a web app.
  virtual std::unique_ptr<WebAppIconDownloader> CreateIconDownloader();

  using AllManifestsCallbackList =
      content::PageManifestManager::AllManifestsCallbackList;
  // This calls PageManifestManager::GetAllSpecifiedManifests on the current
  // primary page of the web contents (see that method for more details about
  // the behavior).
  //
  // Notes:
  // - There is no timeout for this method, it will never be called if no
  //   manifest is specified on the page.
  // - The callback is called synchronously if the manifest is already known,
  //   and then asynchronously for any subsequent manifests found.
  // - In the future it might be appropriate to move this method to the
  //   WebAppDataRetriever, but since that increases WebAppTabHelper complexity,
  //   it lives on the WebContentsManager for now.
  virtual base::CallbackListSubscription GetPrimaryPageAllSpecifiedManifests(
      content::WebContents& web_contents,
      AllManifestsCallbackList::CallbackType callback);

  // Safely downcast to the fake version for tests.
  virtual FakeWebContentsManager* AsFakeWebContentsManagerForTesting();

  base::WeakPtr<WebContentsManager> GetWeakPtr();

 private:
  base::WeakPtrFactory<WebContentsManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_
