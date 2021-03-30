// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_EXTENSION_LOAD_WAITER_ONE_SHOT_H_
#define CHROME_TEST_BASE_EXTENSION_LOAD_WAITER_ONE_SHOT_H_

#include "base/callback_forward.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/test/test_utils.h"

namespace content {
class BrowserContext;
}  // namespace content

// A class used to wait for an extension to load. Callers provide a load
// callback and can block until the extension loads.
class ExtensionLoadWaiterOneShot : public content::NotificationObserver {
 public:
  ExtensionLoadWaiterOneShot();
  ExtensionLoadWaiterOneShot(const ExtensionLoadWaiterOneShot&) = delete;
  ExtensionLoadWaiterOneShot& operator=(const ExtensionLoadWaiterOneShot&) =
      delete;
  ~ExtensionLoadWaiterOneShot() override;

  // Waits for extension with |extension_id| to load. The id should be a pointer
  // to a static char array.
  void WaitForExtension(const char* extension_id, base::OnceClosure load_cb);

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Get the browser context associated with the loaded extension. Returns
  // NULL if |WaitForExtension| was not previously called.
  content::BrowserContext* browser_context() { return browser_context_; }

  // Get the id of the loaded extension.
  const char* extension_id() const { return extension_id_; }

 private:
  content::NotificationRegistrar registrar_;
  scoped_refptr<content::MessageLoopRunner> load_looper_;
  const char* extension_id_ = nullptr;
  content::BrowserContext* browser_context_ = nullptr;
};

#endif  // CHROME_TEST_BASE_EXTENSION_LOAD_WAITER_ONE_SHOT_H_
