// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class OmniboxPopupPresenterBase;
class OmniboxPopupUI;
template <typename T>
class WebUIContentsWrapperT;

// The pool of WebUI omnibox popups that a profile's browser windows share. It
// tracks the presenters that share their popup, so that a window can take an
// idle popup of the same kind from another window instead of loading a new
// WebContents. Each popup in use stays owned by its window's presenter. Used
// only with `omnibox::kOmniboxWebUIPopupShareAcrossWindows`. Created by
// `OmniboxPopupWebUIPoolFactory`.
class OmniboxPopupWebUIPool : public KeyedService {
 public:
  OmniboxPopupWebUIPool();
  OmniboxPopupWebUIPool(const OmniboxPopupWebUIPool&) = delete;
  OmniboxPopupWebUIPool& operator=(const OmniboxPopupWebUIPool&) = delete;
  ~OmniboxPopupWebUIPool() override;

  base::WeakPtr<OmniboxPopupWebUIPool> GetWeakPtr();

  // `presenter` must be removed before it is destroyed.
  void AddPresenter(OmniboxPopupPresenterBase* presenter);
  void RemovePresenter(OmniboxPopupPresenterBase* presenter);

  // Returns whether a presenter other than `presenter`, of the same popup
  // kind, has WebUI content.
  bool HasWebUIContent(const OmniboxPopupPresenterBase& presenter) const;

  // Releases the WebContents of an idle popup of the same kind as `presenter`
  // in another browser window, or returns null if there is none.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> TakeIdleWebUIContents(
      const OmniboxPopupPresenterBase& presenter);

 private:
  std::vector<raw_ptr<OmniboxPopupPresenterBase>> presenters_;
  base::WeakPtrFactory<OmniboxPopupWebUIPool> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_
