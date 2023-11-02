// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_H_

namespace web {
class BrowserState;
}  // namespace web

// Manages the active state associated with a particular BrowserState. Not
// thread safe. Must be used only on the main thread.
class ActiveStateManager {
 public:
  // Returns whether |browser_state| has an associated ActiveStateManager.
  // Must only be accessed from main thread.
  static bool ExistsForBrowserState(web::BrowserState* browser_state);

  // Returns the ActiveStateManager associated with |browser_state.|
  // Lazily creates one if an ActiveStateManager is not already associated with
  // the |browser_state|. |browser_state| cannot be a nullptr.  Must be accessed
  // only from the main thread.
  static ActiveStateManager* FromBrowserState(web::BrowserState* browser_state);

  // Sets the active state of the ActiveStateManager. At most one
  // ActiveStateManager can be active at any given time in the app. A
  // ActiveStateManager must be made inactive before it is destroyed. It is
  // valid to call |SetActive(true)| on an already active ActiveStateManager.
  virtual void SetActive(bool active) = 0;
  // Returns true if the BrowserState is active.
  virtual bool IsActive() = 0;

  // Observer that is notified when a ActiveStateManager becomes active,
  // inactive or destroyed.
  class Observer {
   public:
    // Called when the ActiveStateManager becomes active.
    virtual void OnActive() {}
    // Called when the ActiveStateManager becomes inactive.
    virtual void OnInactive() {}
    // Called just before the ActiveStateManager is destroyed.
    virtual void WillBeDestroyed() {}
  };
  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~ActiveStateManager() {}
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_H_
