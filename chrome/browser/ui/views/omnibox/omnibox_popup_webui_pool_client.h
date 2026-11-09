// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class OmniboxPopupPresenterBase;
class OmniboxPopupUI;
class OmniboxPopupWebUIBaseContent;
class OmniboxPopupWebUIPool;
class Profile;
template <typename T>
class WebUIContentsWrapperT;

// Connects a presenter that shares its popup to its profile's
// `OmniboxPopupWebUIPool`, and holds the sharing policy, so that the presenter
// only asks it at the points where it creates or hides its popup. Also takes
// an idle popup when the presenter's browser window becomes active, so that
// the popup is in place before the user types.
class OmniboxPopupWebUIPoolClient : public views::WidgetObserver {
 public:
  // Where a presenter's content came from. Recorded in UMA as
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
  // enabled, `presenter` shares its popup kind and `profile` has a pool.
  // `browser_widget` is the browser window that hosts the popup.
  static std::unique_ptr<OmniboxPopupWebUIPoolClient> CreateIfSharingEnabled(
      OmniboxPopupPresenterBase& presenter,
      Profile* profile,
      views::Widget* browser_widget);

  OmniboxPopupWebUIPoolClient(OmniboxPopupPresenterBase& presenter,
                              OmniboxPopupWebUIPool& pool,
                              views::Widget* browser_widget);
  OmniboxPopupWebUIPoolClient(const OmniboxPopupWebUIPoolClient&) = delete;
  OmniboxPopupWebUIPoolClient& operator=(const OmniboxPopupWebUIPoolClient&) =
      delete;
  ~OmniboxPopupWebUIPoolClient() override;

  // Whether another window of the profile has a popup of the same kind, which
  // moves to this window when this window needs it.
  bool HasSharedWebUIContent() const;

  // Releases an idle popup of the same kind from another window of the
  // profile, or returns null if there is none.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
  TakeIdleWebUIContents();

  // Takes an idle popup from another window and makes the presenter adopt it,
  // recording `source` and how long the move took. Returns the presenter's new
  // content, or null if no popup was idle.
  OmniboxPopupWebUIBaseContent* TakeAndAdoptIdleWebUIContents(
      ContentSource source);

  // Called after the presenter created new content.
  void OnWebUIContentCreated();

  void RecordContentSource(ContentSource source);

  // Called after the presenter hid its shown popup. Destroys the idle popups
  // of the same kind in the profile's other windows, keeping this one, so that
  // copies that other windows created while this popup was busy do not stay
  // around.
  void OnPopupHidden();

 private:
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Takes an idle popup from another window if the presenter still has none.
  void TakeIdleWebUIContentsIfNeeded();

  void DropOtherIdleWebUIContents();

  const raw_ref<OmniboxPopupPresenterBase> presenter_;
  // `presenter_->GetSharedPopupKind()`, which the destructor can't read.
  const OmniboxPopupState kind_;

  // The pool outlives the presenter in practice, since browser windows keep
  // their profile alive; the weak pointer makes the order not matter.
  base::WeakPtr<OmniboxPopupWebUIPool> pool_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{this};
  base::WeakPtrFactory<OmniboxPopupWebUIPoolClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_POOL_CLIENT_H_
