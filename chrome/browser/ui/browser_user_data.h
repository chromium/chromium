// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_USER_DATA_H_
#define CHROME_BROWSER_UI_BROWSER_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/browser.h"

// Do not introduce new uses of this class. Instead use BrowserWindowFeatures.
// BrowserWindowFeatures is functionally identical but has two benefits: it does
// not force a dependency onto class Browser, and the lifetime semantics are
// explicit rather than implicit.
//
// For example, the following two getters are equivalent:
//   (1) FooFeature::GetOrCreateForBrowser(browser)
//   (2) browser->browser_window_features()->get_foo_feature().
// In (1), FooFeature depends on Browser. As Browser depends on everything, this
// is a circular dependency. In (2), FooFeature does not have to depend on
// Browser.
//
// A base class for classes attached to, and scoped to, the lifetime of a
// Browser. For example:
//
// --- in foo_helper.h ---
// class FooHelper : public BrowserUserData<FooHelper> {
//  public:
//   ~FooHelper() override;
//
//   // ... more public stuff here ...
//
//  private:
//   explicit FooHelper(Browser* browser);
//
//   friend BrowserUserData;
//   BROWSER_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_helper.cc ---
// BROWSER_USER_DATA_KEY_IMPL(FooHelper)

template <typename T>
class BrowserUserData : public base::SupportsUserData::Data {
 public:
  explicit BrowserUserData(Browser& browser) : browser_(&browser) {}
  BrowserUserData(const BrowserUserData&) = delete;
  BrowserUserData& operator=(const BrowserUserData&) = delete;

  // Creates an object of type T, and attaches it to the specified Browser.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForBrowser(Browser* browser, Args&&... args) {
    DCHECK(browser);
    if (!FromBrowser(browser)) {
      browser->SetUserData(
          UserDataKey(),
          base::WrapUnique(new T(browser, std::forward<Args>(args)...)));
    }
  }

  // Retrieves the instance of type T that was attached to the specified
  // Browser (via CreateForBrowser above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromBrowser(Browser* browser) {
    DCHECK(browser);
    return static_cast<T*>(browser->GetUserData(UserDataKey()));
  }
  static const T* FromBrowser(const Browser* browser) {
    DCHECK(browser);
    return static_cast<const T*>(browser->GetUserData(UserDataKey()));
  }

  static T* GetOrCreateForBrowser(Browser* browser) {
    if (auto* data = FromBrowser(browser)) {
      return data;
    }

    CreateForBrowser(browser);
    return FromBrowser(browser);
  }

  // Removes the instance attached to the specified Browser.
  static void RemoveFromBrowser(Browser* browser) {
    DCHECK(FromBrowser(browser));
    browser->RemoveUserData(UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }

  // Returns the Browser associated with `this` object of a subclass
  // which inherits from BrowserUserData.
  //
  // The returned `Browser` is guaranteed to live as long as `this`
  // BrowserUserData (due to how UserData works - Browser
  // owns `this` UserData).
  Browser& GetBrowser() { return *browser_; }
  const Browser& GetBrowser() const { return *browser_; }

 private:
  // Browser associated with subclass which inherits this BrowserUserData.
  const raw_ptr<Browser> browser_ = nullptr;
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// BrowserUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class.
#define BROWSER_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define BROWSER_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

#endif  // CHROME_BROWSER_UI_BROWSER_USER_DATA_H_
