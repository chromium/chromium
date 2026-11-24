// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class OmniboxPopupWebUIPoolHost;
class OmniboxPopupUI;
class OmniboxPopupWebUIBaseContent;
class OmniboxPopupWebUIPool;
class Profile;
template <typename T>
class WebUIContentsWrapperT;

// Connects a host that shares its popup to its profile's
// `OmniboxPopupWebUIPool`, and holds the sharing policy, so that the host
// only asks it at the points where it creates or hides its popup. Also takes
// an idle popup when the host's browser window becomes active, so that
// the popup is in place before the user types.
class OmniboxPopupWebUIPoolClient : public views::WidgetObserver {
 public:
  // Where a host's content came from. Recorded in UMA as
  // Omnibox.Popup.{WebUIType}.Shared.ContentSource. Do not renumber; keep in
  // sync with OmniboxPopupSharedContentSource in enums.xml.
  // LINT.IfChange(OmniboxPopupSharedContentSource)
  enum class ContentSource {
    // Created because no other window had a popup of the kind.
    kCreated = 0,
    // Created because the last popup of the kind closed with its window.
    kCreatedAfterHolderClosed = 1,
    // Taken from another window when this window needed it.
    kTakenOnDemand = 2,
    // Taken from another window when this window became active.
    kTakenOnActivation = 3,
    // Prewarming skipped because another window had a popup of the kind.
    kPrewarmSkipped = 4,
    // Created because another window's popup of the kind was busy.
    kCreatedWhileOtherBusy = 5,
    kMaxValue = kCreatedWhileOtherBusy,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxPopupSharedContentSource)

  // Returns null unless `omnibox::kOmniboxWebUIPopupShareAcrossWindows` is
  // enabled, `host` shares its popup kind and `profile` has a pool.
  static std::unique_ptr<OmniboxPopupWebUIPoolClient> CreateIfSharingEnabled(
      OmniboxPopupWebUIPoolHost& host,
      Profile* profile);

  OmniboxPopupWebUIPoolClient(OmniboxPopupWebUIPoolHost& host,
                              OmniboxPopupWebUIPool& pool);
  OmniboxPopupWebUIPoolClient(const OmniboxPopupWebUIPoolClient&) = delete;
  OmniboxPopupWebUIPoolClient& operator=(const OmniboxPopupWebUIPoolClient&) =
      delete;
  ~OmniboxPopupWebUIPoolClient() override;

  // Whether another window of the profile has a popup of the same kind, which
  // moves to this window when this window needs it.
  bool HasSharedWebUIContent() const;

  // Returns true, and records that prewarming was skipped, when another window
  // of the profile already has a popup of the same kind.
  bool SkipPrewarmIfSharedContentExists();

  // Releases an idle popup of the same kind from another window of the
  // profile, or returns null if there is none.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
  TakeIdleWebUIContents();

  // Takes an idle popup from another window and makes the host adopt it,
  // recording `source` and how long the move took. Returns the host's new
  // content, or null if no popup was idle.
  OmniboxPopupWebUIBaseContent* TakeAndAdoptIdleWebUIContents(
      ContentSource source);

  // Called after the host created new content.
  void OnWebUIContentCreated();

  void RecordContentSource(ContentSource source);

  // Runs `callback`, which takes or creates the host's content, once no
  // other window's popup of this kind is still loading, so restored windows
  // do not each load one.
  void RequestWebUIContents(base::OnceClosure callback);

  // Runs the pending `RequestWebUIContents()` callback, if any, unless the
  // host has content or another window's popup is still loading.
  // Returns whether it ran the callback.
  bool RunPendingRequestIfReady();

  // Called when the host's page finishes loading, so that a window
  // waiting for it can take it.
  void OnWebUIContentsLoaded();

  // The browser window that hosts the popup, if any.
  views::Widget* browser_widget() {
    return browser_widget_observation_.GetSource();
  }

  // Called after the host hid its shown popup. Destroys the idle popups
  // of the same kind in the profile's other windows, keeping this one, so that
  // copies that other windows created while this popup was busy do not stay
  // around.
  void OnPopupHidden();

 private:
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Takes an idle popup from another window if the host still has none,
  // or runs a pending `RequestWebUIContents()` call.
  void TakeIdleWebUIContentsIfNeeded();

  void DropOtherIdleWebUIContents();

  const raw_ref<OmniboxPopupWebUIPoolHost> host_;
  // `host_->GetSharedPopupKind()`, which the destructor can't read.
  const OmniboxPopupState kind_;

  // The pool outlives the host in practice, since browser windows keep
  // their profile alive; the weak pointer makes the order not matter.
  base::WeakPtr<OmniboxPopupWebUIPool> pool_;

  base::OnceClosure pending_request_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{this};
  base::WeakPtrFactory<OmniboxPopupWebUIPoolClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
