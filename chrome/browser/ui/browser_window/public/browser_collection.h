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

class BrowserCollectionObserver;
class BrowserWindowInterface;

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

  // Iterates over all BrowserWindowInstances in this collection present at
  // the time ForEach() was invoked in a given `order`.
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
               Order order);

 protected:
  BrowserCollection();
  virtual ~BrowserCollection();

  base::ObserverList<BrowserCollectionObserver>& observers() {
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
  friend base::ScopedObservationTraits<BrowserCollection,
                                       BrowserCollectionObserver>;

  base::ObserverList<BrowserCollectionObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_
