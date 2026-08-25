// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_

#include <variant>

#include "base/memory/stack_allocated.h"

class BrowserWindowInterface;
class Profile;

namespace webui {

// PreloadContext wraps a Profile or BrowserWindowInterface under which the
// preload manager determines the best preloading strategy. Currently, this
// class is used only as a Profile wrapper because the preload manager does not
// have access to BrowserWindowInterface when creating WebContents. This may
// change in the future so we prepare for it.
// NOTE: a PreloadContext should NOT outlive the Profile or
// BrowserWindowInterface it wraps.
class PreloadContext {
  STACK_ALLOCATED();

 public:
  ~PreloadContext();

  static PreloadContext From(BrowserWindowInterface* browser);
  static PreloadContext From(Profile* profile);

  BrowserWindowInterface* GetBrowser();
  const BrowserWindowInterface* GetBrowser() const;
  Profile* GetProfile();
  const Profile* GetProfile() const;

  bool IsBrowser() const;
  bool IsProfile() const;

 private:
  PreloadContext();

  // This class should NOT outlive the Profile or BrowserWindowInterface it
  // wraps.
  std::variant<BrowserWindowInterface*, Profile*> store_;
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_
