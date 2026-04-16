// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

namespace {

// Iterates through all browsers in `collection` present at construction time,
// accounting for Browsers that may be destroyed mid-iteration.
class BrowserCollectionEnumerator : public BrowserCollectionObserver {
 public:
  BrowserCollectionEnumerator(BrowserCollection* collection,
                              BrowserCollection::BrowserVector browsers,
                              bool enumerate_new_browsers)
      : enumerate_new_browsers_(enumerate_new_browsers),
        browsers_(std::move(browsers)) {
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
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    if (enumerate_new_browsers_) {
      browsers_.push_back(browser);
    }
  }

  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    // Nullify the closed browser to ensure we skip over it during iteration.
    auto it = std::ranges::find(browsers_, browser);
    if (it != browsers_.end()) {
      *it = nullptr;
    }
  }

  bool enumerate_new_browsers_;
  BrowserCollection::BrowserVector browsers_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};
};

}  // namespace

BrowserCollection::BrowserCollection() = default;

BrowserCollection::~BrowserCollection() = default;

void BrowserCollection::ForEach(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser,
    Order order,
    bool enumerate_new_browsers) {
  BrowserCollectionEnumerator(this, GetBrowsers(order), enumerate_new_browsers)
      .ForEach(on_browser);
}

BrowserWindowInterface* BrowserCollection::GetLastActiveBrowser() {
  auto browsers = GetBrowsers(Order::kActivation);
  return browsers.empty() ? nullptr : browsers.front();
}

BrowserWindowInterface* BrowserCollection::FindBrowserWithWindow(
    gfx::NativeWindow window) {
  if (!window) {
    return nullptr;
  }
  BrowserWindowInterface* found = nullptr;
  ForEach([&found, &window](BrowserWindowInterface* browser) {
    if (browser->GetWindow() &&
        browser->GetWindow()->GetNativeWindow() == window) {
      found = browser;
      return false;
    }
    return true;
  });
  return found;
}

BrowserWindowInterface* BrowserCollection::FindBrowserWithID(
    SessionID desired_id) {
  BrowserWindowInterface* found = nullptr;
  ForEach(
      [&found, desired_id](BrowserWindowInterface* browser) {
        if (browser->GetSessionID() == desired_id) {
          found = browser;
          return false;
        }
        return true;
      },
      Order::kActivation);
  return found;
}

BrowserWindowInterface* BrowserCollection::FindBrowserWithTab(
    const content::WebContents* web_contents) {
  DCHECK(web_contents);
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
      const_cast<content::WebContents*>(web_contents));
  if (!tab) {
    return nullptr;
  }
  BrowserWindowInterface* host_browser = tab->GetBrowserWindowInterface();
  if (!host_browser) {
    return nullptr;
  }

  // Test to see if the host browser belongs to the current collection.
  bool found = false;
  ForEach([&found, host_browser](BrowserWindowInterface* browser) {
    if (host_browser == browser) {
      found = true;
    }
    return !found;
  });
  return found ? host_browser : nullptr;
}

void BrowserCollection::AddObserver(BrowserCollectionObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserCollection::RemoveObserver(BrowserCollectionObserver* observer) {
  observers_.RemoveObserver(observer);
}
