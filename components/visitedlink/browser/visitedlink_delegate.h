// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_
#define COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_

#include "base/memory/ref_counted.h"

class GURL;

namespace net {
class SchemefulSite;
}

namespace url {
class Origin;
}

namespace visitedlink {

// Delegate class that clients of VisitedLinkWriter must implement.
class VisitedLinkDelegate {
 public:
  // See VisitedLinkWriter::RebuildTable.
  class URLEnumerator : public base::RefCountedThreadSafe<URLEnumerator> {
   public:
    // Call this with each URL to rebuild the table.
    virtual void OnURL(const GURL& url) = 0;

    // This must be called by Delegate after RebuildTable is called. |success|
    // indicates all URLs have been returned successfully. The URLEnumerator
    // object cannot be used by the delegate after this call.
    virtual void OnComplete(bool success) = 0;

   protected:
    virtual ~URLEnumerator() = default;

   private:
    friend class base::RefCountedThreadSafe<URLEnumerator>;
  };

  class VisitedLinkEnumerator
      : public base::RefCountedThreadSafe<VisitedLinkEnumerator> {
   public:
    // Call this with each visited link to rebuild the table.
    virtual void OnVisitedLink(const GURL& link_url,
                               const net::SchemefulSite& top_level_site,
                               const url::Origin& frame_origin) = 0;

    // This must be called by Delegate after BuildVisitedLinkTable is called.
    // `success` indicates all visited links have been returned successfully.
    // The VisitedLinkEnumerator object cannot be used by the delegate after
    // this call.
    virtual void OnVisitedLinkComplete(bool success) = 0;

   protected:
    virtual ~VisitedLinkEnumerator() = default;

   private:
    friend class base::RefCountedThreadSafe<VisitedLinkEnumerator>;
  };

  // Delegate class is responsible for persisting the list of visited URLs
  // across browser runs. This is called by VisitedLinkWriter to repopulate
  // its internal table. Note that methods on enumerator can be called on any
  // thread but the delegate is responsible for synchronizating the calls.
  virtual void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) = 0;

  // Delegate class is responsible for persisting the list of partitioned
  // visited links across browser runs. This is called by
  // PartitionVisitedLinkWriter to repopulate its internal table. Note that
  // methods on enumerator can be called on any thread but the delegate is
  // responsible for synchronizating the calls.
  virtual void BuildVisitedLinkTable(
      const scoped_refptr<VisitedLinkEnumerator>& enumerator) = 0;

 protected:
  virtual ~VisitedLinkDelegate() = default;
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_DELEGATE_H_
