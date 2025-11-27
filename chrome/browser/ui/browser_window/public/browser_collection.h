// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_

#include "base/observer_list.h"

class BrowserCollectionObserver;

// A common base class for collections of BrowserWindowInterface objects.
class BrowserCollection {
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

 private:
  base::ObserverList<BrowserCollectionObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_COLLECTION_H_
