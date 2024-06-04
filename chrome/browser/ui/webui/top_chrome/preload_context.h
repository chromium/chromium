// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Browser;
class Profile;

namespace webui {

// PreloadContext wraps a Profile or Browser under which the preload manager
// determines the best preloading strategy.
// Currently, this class is used only as a Profile wrapper because the preload
// manager does not have access to Browser when creating WebContents. This may
// change in the future so we prepare for it.
// NOTE: a PreloadContext should NOT outlive the Profile or Browser it wraps.
class PreloadContext {
 public:
  ~PreloadContext();

  static PreloadContext From(Browser* browser);
  static PreloadContext From(Profile* profile);

  const Browser* GetBrowser() const;
  const Profile* GetProfile() const;

  bool IsBrowser() const;
  bool IsProfile() const;

 private:
  PreloadContext();

  // This class should NOT outlive the Profile or Browser it wraps.
  absl::variant<Browser*, Profile*> store_;
};

}  // namespace webui

#endif  /// CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CONTEXT_H_
