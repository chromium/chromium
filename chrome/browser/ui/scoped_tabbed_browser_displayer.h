// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCOPED_TABBED_BROWSER_DISPLAYER_H_
#define CHROME_BROWSER_UI_SCOPED_TABBED_BROWSER_DISPLAYER_H_

#include "base/memory/raw_ptr.h"

class Browser;
class Profile;

namespace chrome {

// This class finds the last active tabbed browser matching |profile|. If there
// is no tabbed browser and it is possible to create one, a new non visible
// browser is created. ScopedTabbedBrowserDisplayer ensures that the browser is
// made visible and is activated by the time that ScopedTabbedBrowserDisplayer
// goes out of scope.
class ScopedTabbedBrowserDisplayer {
 public:
  explicit ScopedTabbedBrowserDisplayer(Profile* profile);

  ScopedTabbedBrowserDisplayer(const ScopedTabbedBrowserDisplayer&) = delete;
  ScopedTabbedBrowserDisplayer& operator=(const ScopedTabbedBrowserDisplayer&) =
      delete;

  ~ScopedTabbedBrowserDisplayer();

  Browser* browser() { return browser_; }

 private:
  raw_ptr<Browser> browser_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SCOPED_TABBED_BROWSER_DISPLAYER_H_
