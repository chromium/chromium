// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"

// Handles requests for a given tab's thumbnail and watches for thumbnail
// updates for the lifetime of the tab.
class ThumbnailTracker::ContentsData : public content::WebContentsObserver,
                                       public ThumbnailImage::Observer {
 public:
  ContentsData(ThumbnailTracker* parent, content::WebContents* contents)
      : content::WebContentsObserver(contents), parent_(parent) {
    thumbnail_ = parent_->thumbnail_getter_.Run(contents);
    if (thumbnail_)
      observer_.Add(thumbnail_.get());
  }

  void RequestThumbnail() {
    if (thumbnail_)
      thumbnail_->RequestCompressedThumbnailData();
  }

  // content::WebContents:
  void WebContentsDestroyed() override {
    // We must un-observe each ThumbnailImage when the WebContents it came from
    // closes.
    if (thumbnail_) {
      observer_.Remove(thumbnail_.get());
      thumbnail_.reset();
    }

    // Destroy ourself. After this call, |this| doesn't exist!
    parent_->ContentsClosed(web_contents());
  }

  // ThumbnailImage::Observer:
  void OnCompressedThumbnailDataAvailable(
      CompressedThumbnailData thumbnail_image) override {
    parent_->ThumbnailUpdated(web_contents(), thumbnail_image);
  }

 private:
  ThumbnailTracker* parent_;
  scoped_refptr<ThumbnailImage> thumbnail_;
  ScopedObserver<ThumbnailImage, ThumbnailImage::Observer> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentsData);
};

ThumbnailTracker::ThumbnailTracker(ThumbnailUpdatedCallback callback)
    : ThumbnailTracker(std::move(callback),
                       base::Bind(GetThumbnailFromTabHelper)) {}

ThumbnailTracker::ThumbnailTracker(ThumbnailUpdatedCallback callback,
                                   GetThumbnailCallback thumbnail_getter)
    : thumbnail_getter_(std::move(thumbnail_getter)),
      callback_(std::move(callback)) {}

ThumbnailTracker::~ThumbnailTracker() = default;

void ThumbnailTracker::AddTab(content::WebContents* contents) {
  auto data_it = contents_data_.find(contents);
  if (data_it == contents_data_.end()) {
    data_it = contents_data_.emplace_hint(
        data_it, contents, std::make_unique<ContentsData>(this, contents));
  }

  data_it->second->RequestThumbnail();
}

void ThumbnailTracker::RemoveTab(content::WebContents* contents) {
  contents_data_.erase(contents);
}

void ThumbnailTracker::ThumbnailUpdated(content::WebContents* contents,
                                        CompressedThumbnailData image) {
  callback_.Run(contents, image);
}

void ThumbnailTracker::ContentsClosed(content::WebContents* contents) {
  contents_data_.erase(contents);
}

// static
scoped_refptr<ThumbnailImage> ThumbnailTracker::GetThumbnailFromTabHelper(
    content::WebContents* contents) {
  ThumbnailTabHelper* thumbnail_helper =
      ThumbnailTabHelper::FromWebContents(contents);
  // Gracefully handle when ThumbnailTabHelper isn't available.
  if (thumbnail_helper) {
    auto thumbnail = thumbnail_helper->thumbnail();
    DCHECK(thumbnail);
    return thumbnail;
  } else {
    DVLOG(1) << "ThumbnailTabHelper doesn't exist for tab";
    return nullptr;
  }
}
