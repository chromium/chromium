// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_limit.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace views {
class Widget;
}  // namespace views

class OmniboxPopupPresenterBase;
class OmniboxPopupUI;
template <typename T>
class WebUIContentsWrapperT;

// The pool of WebUI omnibox popups that a profile's browser windows share. It
// tracks the presenters that share their popup, so that a window can take an
// idle popup of the same kind from another window instead of loading a new
// WebContents. Each popup in use stays owned by its window's presenter. Used
// only with `omnibox::kOmniboxWebUIPopupShareAcrossWindows`. Created by
// `OmniboxPopupWebUIPoolFactory`. With
// `omnibox::kOmniboxWebUIPopupShareDropIdleOnMemoryPressure`, it destroys the
// idle popups under memory pressure.
class OmniboxPopupWebUIPool : public KeyedService, public base::MemoryConsumer {
 public:
  OmniboxPopupWebUIPool();
  OmniboxPopupWebUIPool(const OmniboxPopupWebUIPool&) = delete;
  OmniboxPopupWebUIPool& operator=(const OmniboxPopupWebUIPool&) = delete;
  ~OmniboxPopupWebUIPool() override;

  base::WeakPtr<OmniboxPopupWebUIPool> GetWeakPtr();

  // `presenter` must be removed before it is destroyed.
  void AddPresenter(OmniboxPopupPresenterBase* presenter);
  // `kind` is the presenter's `GetSharedPopupKind()`, which can't be read
  // while the presenter is being destroyed.
  void RemovePresenter(OmniboxPopupPresenterBase* presenter,
                       OmniboxPopupState kind);

  // Returns whether a presenter other than `presenter`, of the same popup
  // kind, has WebUI content.
  bool HasWebUIContent(const OmniboxPopupPresenterBase& presenter) const;

  // Releases the WebContents of an idle popup of the same kind as `presenter`
  // in another browser window, or returns null if there is none.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> TakeIdleWebUIContents(
      const OmniboxPopupPresenterBase& presenter);

  // Destroys the idle popups of the same kind as `presenter` in other browser
  // windows. Returns how many it destroyed.
  size_t DropOtherIdleWebUIContents(const OmniboxPopupPresenterBase& presenter);

  // Returns whether the last popup of `kind` was destroyed with its browser
  // window since the last call for `kind`.
  bool ConsumeHolderClosed(OmniboxPopupState kind);

  // Records the browser window of the profile that was activated last.
  void OnBrowserWindowActivated(views::Widget* browser_widget);

  // Destroys the idle popups. A classic or full popup in the browser window
  // that was activated last is kept until it has been hidden for
  // `omnibox::kOmniboxWebUIPopupShareActiveWindowPopupIdleTimeout`: the user is
  // likely to open it again soon, and memory pressure that persists would
  // otherwise reload it on every use. AI Mode popups are opened less often, so
  // they are always dropped.
  void DropIdleWebUIContents();

  // Returns whether memory pressure at `limit` drops the idle popups.
  static bool ShouldDropIdleWebUIContents(base::MemoryLimit limit);

  // base::MemoryConsumer:
  void OnReleaseMemory() override;
  void OnUpdateMemoryLimit() override;

 private:
  std::vector<raw_ptr<OmniboxPopupPresenterBase>> presenters_;
  base::flat_set<OmniboxPopupState> holder_closed_kinds_;
  base::WeakPtr<views::Widget> last_active_browser_widget_;
  std::optional<base::MemoryConsumerRegistration> memory_consumer_registration_;
  base::WeakPtrFactory<OmniboxPopupWebUIPool> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_H_
