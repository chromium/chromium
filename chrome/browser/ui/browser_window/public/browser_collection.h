// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_

#include <vector>

#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "components/sessions/core/session_id.h"
#include "ui/gfx/native_ui_types.h"

class BrowserCollectionObserver;
class BrowserWindowInterface;

namespace content {
class WebContents;
}

// A common base class for collections of BrowserWindowInterface objects.
class BrowserCollection {
 public:
  using BrowserVector = std::vector<raw_ptr<BrowserWindowInterface>>;

  // Defines the order in which browsers are iterated.
  // `kCreation` will iterate over browsers starting from the least recently
  // created to the most recently created.
  // `kActivation` will iterate over browsers starting from the most recently
  // activated to the least recently activated.
  enum class Order {
    kCreation,
    kActivation,
  };

  // Iterates over all BrowserWindowInstances in this collection present at the
  // time ForEach() was invoked in a given `order`. If `enumerate_new_browsers`
  // is true browsers created during iteration will be appended to the end of
  // the current list of browsers in the order they were created.
  //
  // The return value in the passed-in function indicates whether or not we
  // should continue iterating - true means continue, false means terminate.
  //
  // Example usage:
  //
  //   browser_collection->ForEach(
  //       [](BrowserWindowInterface* browser) {
  //         // Do something with `browser`.
  //         return true;
  //       });
  void ForEach(base::FunctionRef<bool(BrowserWindowInterface*)> on_browser,
               Order order = Order::kCreation,
               bool enumerate_new_browsers = false);

  // True if there are no BrowserWindowInterfaces belonging to this collection.
  virtual bool IsEmpty() const = 0;

  // Returns the number of BrowserWindowInterfaces belonging to this collection.
  virtual size_t GetSize() const = 0;

  // Gets the last active browser for this collection.
  BrowserWindowInterface* GetLastActiveBrowser();

  // Returns the browser represented by `window`. Returns nullptr if no such
  // browser currently exists in this collection.
  BrowserWindowInterface* FindBrowserWithWindow(gfx::NativeWindow window);

  // Finds a browser by its session ID. Returns nullptr if no browser with the
  // given ID exists in this collection.
  BrowserWindowInterface* FindBrowserWithID(SessionID desired_id);

  // Returns the browser containing the specified `web_contents` as a tab.
  // Returns nullptr if no such browser exists in this collection.
  // `web_contents` must not be nullptr.
  BrowserWindowInterface* FindBrowserWithTab(
      const content::WebContents* web_contents);

 protected:
  BrowserCollection();
  virtual ~BrowserCollection();

  base::ObserverList<
      BrowserCollectionObserver,
      /*check_empty=*/false,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>&
  observers() {
    return observers_;
  }

  // Protected to ensure clients only register observations on BrowserCollection
  // subclasses via ScopedObservation.
  void AddObserver(BrowserCollectionObserver* observer);
  void RemoveObserver(BrowserCollectionObserver* observer);

  // Subclasses must return a vector of their collection's Browsers in the
  // specified `order`.
  virtual BrowserVector GetBrowsers(Order order) = 0;

 private:
  // TODO(crbug.com/c/500850766): Remove this once ChromeTracingDelegate
  // supports being a global feature.
  friend class ChromeTracingDelegate;
  friend base::ScopedObservationTraits<BrowserCollection,
                                       BrowserCollectionObserver>;

  // TODO(crbug.com/484371187): Investigate if reentrancy can be removed.
  base::ObserverList<
      BrowserCollectionObserver,
      /*check_empty=*/false,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observers_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_
