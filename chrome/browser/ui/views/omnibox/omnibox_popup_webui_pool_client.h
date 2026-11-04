// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

class OmniboxPopupPresenterBase;
class OmniboxPopupUI;
class OmniboxPopupWebUIPool;
class Profile;
template <typename T>
class WebUIContentsWrapperT;

// Connects a presenter that shares its popup to its profile's
// `OmniboxPopupWebUIPool`, and holds the sharing policy, so that the presenter
// only asks it at the points where it creates or hides its popup.
class OmniboxPopupWebUIPoolClient {
 public:
  // Returns null unless `omnibox::kOmniboxWebUIPopupShareAcrossWindows` is
  // enabled, `presenter` shares its popup kind and `profile` has a pool.
  static std::unique_ptr<OmniboxPopupWebUIPoolClient> CreateIfSharingEnabled(
      OmniboxPopupPresenterBase& presenter,
      Profile* profile);

  OmniboxPopupWebUIPoolClient(OmniboxPopupPresenterBase& presenter,
                              OmniboxPopupWebUIPool& pool);
  OmniboxPopupWebUIPoolClient(const OmniboxPopupWebUIPoolClient&) = delete;
  OmniboxPopupWebUIPoolClient& operator=(const OmniboxPopupWebUIPoolClient&) =
      delete;
  ~OmniboxPopupWebUIPoolClient();

  // Whether another window of the profile has a popup of the same kind, which
  // moves to this window when this window needs it.
  bool HasSharedWebUIContent() const;

  // Releases an idle popup of the same kind from another window of the
  // profile, or returns null if there is none.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
  TakeIdleWebUIContents();

 private:
  const raw_ref<OmniboxPopupPresenterBase> presenter_;

  // The pool outlives the presenter in practice, since browser windows keep
  // their profile alive; the weak pointer makes the order not matter.
  base::WeakPtr<OmniboxPopupWebUIPool> pool_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
