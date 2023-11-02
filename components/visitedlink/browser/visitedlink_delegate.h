// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_
#define COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_

#include "base/memory/ref_counted.h"

class GURL;

namespace visitedlink {

// Delegate class that clients of VisitedLinkWriter must implement.
class VisitedLinkDelegate {
 public:
  // See RebuildTable.
  class URLEnumerator : public base::RefCountedThreadSafe<URLEnumerator> {
   public:
    // Call this with each URL to rebuild the table.
    virtual void OnURL(const GURL& url) = 0;

    // This must be called by Delegate after RebuildTable is called. |success|
    // indicates all URLs have been returned successfully. The URLEnumerator
    // object cannot be used by the delegate after this call.
    virtual void OnComplete(bool success) = 0;

   protected:
    virtual ~URLEnumerator() {}

   private:
    friend class base::RefCountedThreadSafe<URLEnumerator>;
  };

  // Delegate class is responsible for persisting the list of visited URLs
  // across browser runs. This is called by VisitedLinkWriter to repopulate
  // its internal table. Note that methods on enumerator can be called on any
  // thread but the delegate is responsible for synchronizating the calls.
  virtual void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) = 0;

 protected:
  virtual ~VisitedLinkDelegate() {}
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_
