// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_client_fake_bookmarks.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_backend_client.h"
#include "url/gurl.h"

namespace history {

class FakeBookmarkDatabase
    : public base::RefCountedThreadSafe<FakeBookmarkDatabase> {
 public:
  FakeBookmarkDatabase() {}

  FakeBookmarkDatabase(const FakeBookmarkDatabase&) = delete;
  FakeBookmarkDatabase& operator=(const FakeBookmarkDatabase&) = delete;

  void ClearAllBookmarks();
  void AddBookmarkWithTitle(const GURL& url, const std::u16string& title);
  void DelBookmark(const GURL& url);

  bool IsBookmarked(const GURL& url);
  std::vector<URLAndTitle> GetBookmarks();

 private:
  friend class base::RefCountedThreadSafe<FakeBookmarkDatabase>;

  ~FakeBookmarkDatabase() {}

  base::Lock lock_;
  std::map<GURL, std::u16string> bookmarks_;
};

void FakeBookmarkDatabase::ClearAllBookmarks() {
  base::AutoLock with_lock(lock_);
  bookmarks_.clear();
}

void FakeBookmarkDatabase::AddBookmarkWithTitle(const GURL& url,
                                                const std::u16string& title) {
  base::AutoLock with_lock(lock_);
  bookmarks_.insert(std::make_pair(url, title));
}

void FakeBookmarkDatabase::DelBookmark(const GURL& url) {
  base::AutoLock with_lock(lock_);
  auto iter = bookmarks_.find(url);
  if (iter != bookmarks_.end())
    bookmarks_.erase(iter);
}

bool FakeBookmarkDatabase::IsBookmarked(const GURL& url) {
  base::AutoLock with_lock(lock_);
  return bookmarks_.find(url) != bookmarks_.end();
}

std::vector<URLAndTitle> FakeBookmarkDatabase::GetBookmarks() {
  base::AutoLock with_lock(lock_);
  std::vector<URLAndTitle> result;
  result.reserve(bookmarks_.size());
  for (const auto& pair : bookmarks_) {
    result.push_back(URLAndTitle{pair.first, pair.second});
  }
  return result;
}

namespace {

class HistoryBackendClientFakeBookmarks : public HistoryBackendClient {
 public:
  explicit HistoryBackendClientFakeBookmarks(
      const scoped_refptr<FakeBookmarkDatabase>& bookmarks);

  HistoryBackendClientFakeBookmarks(const HistoryBackendClientFakeBookmarks&) =
      delete;
  HistoryBackendClientFakeBookmarks& operator=(
      const HistoryBackendClientFakeBookmarks&) = delete;

  ~HistoryBackendClientFakeBookmarks() override;

  // HistoryBackendClient implementation.
  bool IsPinnedURL(const GURL& url) override;
  std::vector<URLAndTitle> GetPinnedURLs() override;
  bool IsWebSafe(const GURL& url) override;

 private:
  scoped_refptr<FakeBookmarkDatabase> bookmarks_;
};

HistoryBackendClientFakeBookmarks::HistoryBackendClientFakeBookmarks(
    const scoped_refptr<FakeBookmarkDatabase>& bookmarks)
    : bookmarks_(bookmarks) {
}

HistoryBackendClientFakeBookmarks::~HistoryBackendClientFakeBookmarks() {
}

bool HistoryBackendClientFakeBookmarks::IsPinnedURL(const GURL& url) {
  return bookmarks_->IsBookmarked(url);
}

std::vector<URLAndTitle> HistoryBackendClientFakeBookmarks::GetPinnedURLs() {
  return bookmarks_->GetBookmarks();
}

bool HistoryBackendClientFakeBookmarks::IsWebSafe(const GURL& url) {
  return true;
}

}  // namespace

HistoryClientFakeBookmarks::HistoryClientFakeBookmarks() {
  bookmarks_ = new FakeBookmarkDatabase;
}

HistoryClientFakeBookmarks::~HistoryClientFakeBookmarks() {
}

void HistoryClientFakeBookmarks::ClearAllBookmarks() {
  bookmarks_->ClearAllBookmarks();
}

void HistoryClientFakeBookmarks::AddBookmark(const GURL& url) {
  bookmarks_->AddBookmarkWithTitle(url, std::u16string());
}

void HistoryClientFakeBookmarks::AddBookmarkWithTitle(
    const GURL& url,
    const std::u16string& title) {
  bookmarks_->AddBookmarkWithTitle(url, title);
}

void HistoryClientFakeBookmarks::DelBookmark(const GURL& url) {
  bookmarks_->DelBookmark(url);
}

bool HistoryClientFakeBookmarks::IsBookmarked(const GURL& url) {
  return bookmarks_->IsBookmarked(url);
}

void HistoryClientFakeBookmarks::OnHistoryServiceCreated(
    HistoryService* history_service) {
}

void HistoryClientFakeBookmarks::Shutdown() {
}

CanAddURLCallback HistoryClientFakeBookmarks::GetThreadSafeCanAddURLCallback()
    const {
  return base::BindRepeating([](const GURL& url) { return url.is_valid(); });
}

void HistoryClientFakeBookmarks::NotifyProfileError(
    sql::InitStatus init_status,
    const std::string& diagnostics) {}

std::unique_ptr<HistoryBackendClient>
HistoryClientFakeBookmarks::CreateBackendClient() {
  return std::make_unique<HistoryBackendClientFakeBookmarks>(bookmarks_);
}

void HistoryClientFakeBookmarks::UpdateBookmarkLastUsedTime(
    int64_t bookmark_node_id,
    base::Time time) {}

}  // namespace history
