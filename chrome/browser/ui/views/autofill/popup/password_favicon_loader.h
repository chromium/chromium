// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace base {
class CancelableTaskTracker;
}

namespace favicon {
class LargeIconService;
}

namespace gfx {
class Image;
}
namespace autofill {

// This class handles the process of loading favicons for websites associated
// with user credentials.
class PasswordFaviconLoader {
 public:
  using OnLoadSuccess = base::OnceCallback<void(const gfx::Image& image)>;
  using OnLoadFail = base::OnceCallback<void()>;

  virtual void Load(const Suggestion::FaviconDetails& favicon_details,
                    base::CancelableTaskTracker* task_tracker,
                    OnLoadSuccess on_success,
                    OnLoadFail on_fail) = 0;
};

class PasswordFaviconLoaderImpl : public PasswordFaviconLoader {
 public:
  explicit PasswordFaviconLoaderImpl(
      favicon::LargeIconService& favicon_service);
  virtual ~PasswordFaviconLoaderImpl();

  void Load(const Suggestion::FaviconDetails& favicon_details,
            base::CancelableTaskTracker* task_tracker,
            OnLoadSuccess on_success,
            OnLoadFail on_fail) override;

 private:
  const raw_ref<favicon::LargeIconService> favicon_service_;

  // TODO(crbug.com/325246516): Add cache for loaded images.
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_
