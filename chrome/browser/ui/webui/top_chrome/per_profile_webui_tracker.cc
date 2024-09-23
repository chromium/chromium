// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace {

// The impl class that implements PerProfileWebUITracker.
// PerProfileWebUITracker is an abstract class so that it can be easily mocked
// in tests.
class PerProfileWebUITrackerImpl : public PerProfileWebUITracker {
 public:
  PerProfileWebUITrackerImpl() = default;
  ~PerProfileWebUITrackerImpl() override = default;
  explicit PerProfileWebUITrackerImpl(const PerProfileWebUITracker&) = delete;
  PerProfileWebUITrackerImpl& operator=(const PerProfileWebUITrackerImpl&) =
      delete;

  // PerProfileWebUITracker:
  void AddWebContents(content::WebContents* web_contents) override;
  bool ProfileHasWebUI(Profile* profile, std::string webui_url) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  class WebContentsObserver;

  // The nested WebContentsObserver notifies the impl about WebContents state
  // change using the following functions.
  void OnWebContentsDestroyed(content::WebContents* web_contents);
  void OnWebContentsOriginChanged(content::WebContents* web_contents,
                                  url::Origin old_origin,
                                  url::Origin new_origin);
  void OnWebContentsPrimaryPageChanged(content::WebContents* web_contents);

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  // Observers of tracked WebContents.
  std::map<raw_ptr<content::WebContents>, std::unique_ptr<WebContentsObserver>>
      web_contents_observers_;
  // Maintains a multi-set of {profile, origin} pairs. Note that a multi-set is
  // used because a profile can have multiple browser windows, each can have a
  // WebUI, therefore there can be multiple WebUIs of the same URL under a
  // Profile.
  std::multiset<std::pair<raw_ptr<content::BrowserContext>, url::Origin>>
      profile_origin_set_;
};

// We need an observer instance for each WebContents because methods in
// WebContentsObserver does not provide a pointer to the WebContents itself. If
// we observe WebContents in the tracker class we will be unable to distinguish
// the source WebContents.
class PerProfileWebUITrackerImpl::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(PerProfileWebUITrackerImpl* owner,
                               content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        owner_(owner),
        web_contents_(web_contents),
        origin_(url::Origin::Create(web_contents->GetVisibleURL())) {
    // The WebContents can already be navigated to a WebUI.
    // Update the owner with the initial origin.
    owner_->OnWebContentsOriginChanged(web_contents, url::Origin(), origin_);
  }
  ~WebContentsObserver() override = default;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // The PrimaryPageChanged() is not called during WebContents destroy,
    // so we notify the owner explicitly.
    owner_->OnWebContentsOriginChanged(web_contents_, origin_, url::Origin());
    owner_->OnWebContentsDestroyed(web_contents_);
  }

  void PrimaryPageChanged(content::Page&) override {
    owner_->OnWebContentsPrimaryPageChanged(web_contents_);
    url::Origin new_origin =
        url::Origin::Create(web_contents_->GetVisibleURL());
    if (new_origin != origin_) {
      owner_->OnWebContentsOriginChanged(web_contents_, origin_, new_origin);
      origin_ = new_origin;
    }
  }

 private:
  raw_ptr<PerProfileWebUITrackerImpl> owner_;
  raw_ptr<content::WebContents> web_contents_;
  url::Origin origin_;
};

void PerProfileWebUITrackerImpl::AddWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  CHECK(!web_contents_observers_.contains(web_contents));
  web_contents_observers_.emplace(
      web_contents, std::make_unique<WebContentsObserver>(this, web_contents));
}

bool PerProfileWebUITrackerImpl::ProfileHasWebUI(Profile* profile,
                                                 std::string webui_url) const {
  url::Origin webui_origin = url::Origin::Create(GURL(webui_url));
  return profile_origin_set_.contains({profile, webui_origin});
}

void PerProfileWebUITrackerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PerProfileWebUITrackerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PerProfileWebUITrackerImpl::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  CHECK(web_contents_observers_.contains(web_contents));
  web_contents_observers_.erase(web_contents);

  for (Observer& observer : observers_) {
    observer.OnWebContentsDestroyed(web_contents);
  }
}

void PerProfileWebUITrackerImpl::OnWebContentsOriginChanged(
    content::WebContents* web_contents,
    url::Origin old_origin,
    url::Origin new_origin) {
  content::BrowserContext* profile = web_contents->GetBrowserContext();
  // Opaque origins are about:blank, we don't track them.
  if (!old_origin.opaque()) {
    auto it = profile_origin_set_.find({profile, old_origin});
    CHECK(it != profile_origin_set_.end());
    profile_origin_set_.erase(it);
  }
  if (!new_origin.opaque()) {
    profile_origin_set_.emplace(profile, new_origin);
  }
}

void PerProfileWebUITrackerImpl::OnWebContentsPrimaryPageChanged(
    content::WebContents* web_contents) {
  CHECK(web_contents_observers_.contains(web_contents));
  for (Observer& observer : observers_) {
    observer.OnWebContentsPrimaryPageChanged(web_contents);
  }
}

}  // namespace

// static
std::unique_ptr<PerProfileWebUITracker> PerProfileWebUITracker::Create() {
  return std::make_unique<PerProfileWebUITrackerImpl>();
}
