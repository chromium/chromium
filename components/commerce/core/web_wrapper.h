// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
#define COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace commerce {

// A wrapper class for WebContent on desktop and android or WebState on iOS.
class WebWrapper {
 public:
  WebWrapper();
  WebWrapper(const WebWrapper&) = delete;
  virtual ~WebWrapper();

  // Get the URL that is currently being displayed for the page.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Gets the title for the current page.
  virtual const std::u16string& GetTitle() = 0;

  // Whether the first load after a navigation has completed. This is useful
  // for determining if it is safe to run javascript and whether a navigation
  // was inside of a single-page webapp.
  virtual bool IsFirstLoadForNavigationFinished() = 0;

  // Whether content is off the record or in incognito mode.
  virtual bool IsOffTheRecord() = 0;

  // Execute the provided |script| and pass the result through |callback|. This
  // will run in an isolated world if possible.
  virtual void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) = 0;

  // Get the source ID for the current page.
  virtual ukm::SourceId GetPageUkmSourceId() = 0;

  // Gets a weak pointer for use in callbacks.
  base::WeakPtr<WebWrapper> GetWeakPtr();

 private:
  base::WeakPtrFactory<WebWrapper> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
