// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_

#include <memory>
#include "base/memory/weak_ptr.h"

namespace webapps {
class WebAppUrlLoader;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppIconDownloader;

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

  virtual std::unique_ptr<webapps::WebAppUrlLoader> CreateUrlLoader();

  virtual std::unique_ptr<WebAppDataRetriever> CreateDataRetriever();

  virtual std::unique_ptr<WebAppIconDownloader> CreateIconDownloader();

  base::WeakPtr<WebContentsManager> GetWeakPtr();

 private:
  base::WeakPtrFactory<WebContentsManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_CONTENTS_MANAGER_H_
