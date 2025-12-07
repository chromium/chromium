// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

namespace {

// Iterates through all browsers in `collection` present at construction time,
// accounting for Browsers that may be destroyed mid-iteration.
class BrowserCollectionEnumerator : public BrowserCollectionObserver {
 public:
  BrowserCollectionEnumerator(BrowserCollection* collection,
                              BrowserCollection::BrowserVector browsers)
      : browsers_(std::move(browsers)) {
    browser_collection_observer_.Observe(collection);
  }
  BrowserCollectionEnumerator(const BrowserCollectionEnumerator&) = delete;
  BrowserCollectionEnumerator& operator=(const BrowserCollectionEnumerator&) =
      delete;
  ~BrowserCollectionEnumerator() override = default;

  void ForEach(base::FunctionRef<bool(BrowserWindowInterface*)> on_browser) {
    for (size_t index = 0; index < browsers_.size(); index++) {
      if (browsers_[index] && !on_browser(browsers_[index])) {
        return;
      }
    }
  }

 private:
  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    // Nullify the closed browser to ensure we skip over it during iteration.
    auto it = std::ranges::find(browsers_, browser);
    if (it != browsers_.end()) {
      *it = nullptr;
    }
  }

  BrowserCollection::BrowserVector browsers_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};
};

}  // namespace

BrowserCollection::BrowserCollection() = default;

BrowserCollection::~BrowserCollection() = default;

void BrowserCollection::ForEach(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser,
    Order order) {
  BrowserCollectionEnumerator(this, GetBrowsers(order)).ForEach(on_browser);
}

void BrowserCollection::AddObserver(BrowserCollectionObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserCollection::RemoveObserver(BrowserCollectionObserver* observer) {
  observers_.RemoveObserver(observer);
}
