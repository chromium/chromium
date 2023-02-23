// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"

// Handles requests for a given tab's thumbnail and watches for thumbnail
// updates for the lifetime of the tab.
class ThumbnailTracker::ContentsData : public content::WebContentsObserver {
 public:
  ContentsData(ThumbnailTracker* parent, content::WebContents* contents)
      : content::WebContentsObserver(contents), parent_(parent) {
    thumbnail_ = parent_->thumbnail_getter_.Run(contents);
    if (!thumbnail_)
      return;

    subscription_ = thumbnail_->Subscribe();
    subscription_->SetCompressedImageCallback(base::BindRepeating(
        &ContentsData::ThumbnailImageCallback, base::Unretained(this)));
  }

  ContentsData(const ContentsData&) = delete;
  ContentsData& operator=(const ContentsData&) = delete;

  void RequestThumbnail() {
    if (thumbnail_)
      thumbnail_->RequestCompressedThumbnailData();
  }

  // content::WebContents:
  void WebContentsDestroyed() override {
    // We must un-observe each ThumbnailImage when the WebContents it came from
    // closes.
    if (thumbnail_) {
      subscription_.reset();
      thumbnail_.reset();
    }

    // Destroy ourself. After this call, |this| doesn't exist!
    parent_->ContentsClosed(web_contents());
  }

 private:
  void ThumbnailImageCallback(CompressedThumbnailData image) {
    parent_->ThumbnailUpdated(web_contents(), image);
  }

  raw_ptr<ThumbnailTracker> parent_;
  scoped_refptr<ThumbnailImage> thumbnail_;
  std::unique_ptr<ThumbnailImage::Subscription> subscription_;
};

ThumbnailTracker::ThumbnailTracker(ThumbnailUpdatedCallback callback)
    : ThumbnailTracker(std::move(callback),
                       base::BindRepeating(GetThumbnailFromTabHelper)) {}

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
