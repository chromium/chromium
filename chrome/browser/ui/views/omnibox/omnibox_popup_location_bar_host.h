// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class LocationBarView;
class OmniboxController;
class OmniboxFullPopupWebUIContent;

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

// An `OmniboxPopupHost` that hosts `OmniboxFullPopupWebUIContent` (the same
// content view the popup widget uses) directly inside `LocationBarView`,
// without a popup widget. The content fills the location bar, so the page's
// suggestions dropdown is clipped to the location bar.
// `OmniboxPopupViewFullWebUI` keeps the input sync, tab state and focus
// handling; this host shows, hides, sizes and focuses the content. Gated
// behind `omnibox::ShouldHostUnboundedOmniboxInLocationBar()`.
class OmniboxPopupLocationBarHost : public OmniboxPopupHost,
                                    public OmniboxPopupContentDelegate,
                                    public views::ViewObserver {
 public:
  OmniboxPopupLocationBarHost(LocationBarView& location_bar,
                              OmniboxController& controller);
  OmniboxPopupLocationBarHost(const OmniboxPopupLocationBarHost&) = delete;
  OmniboxPopupLocationBarHost& operator=(const OmniboxPopupLocationBarHost&) =
      delete;
  ~OmniboxPopupLocationBarHost() override;

  // OmniboxPopupHost:
  void Show() override;
  void Hide() override;
  void RequestFocus() override;
  bool IsShown() const override;
  OmniboxPopupWebUIBaseContent* GetWebUIContent() override;
  void OnOmniboxFocused() override {}
  void SetPermissionPromptShowing(bool showing) override;
  bool IsClosePrevented() const override;
  bool CompletedFirstVisuallyNonEmptyPaint() override;
  // No-op: the content fills LocationBarView, so it caches no content height.
  void ResetContentHeightForTabSwitch() override {}
  void DeactivateAndKillFocus() override;
  std::optional<FocusStateToRestore> TakeFocusStateToRestoreAfterModalDialog()
      override;

  // OmniboxPopupContentDelegate:
  bool ShouldReceiveFocus() const override;
  void NotifyEscapeKeyPressed() override {}
  std::unique_ptr<OmniboxPopupDeactivationBlocker> CreateDeactivationBlocker()
      override;
  void OnFileSelectionClosed() override {}

  // OmniboxPopupContentDelegate (SearchboxHandler::Delegate):
  void OnEmbeddedPermissionDialogChanged(
      bool is_showing,
      const gfx::Size& prompt_size) override {}
  OmniboxController* GetOmniboxController() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  // Sizes the content to `LocationBarView`'s local bounds and keeps it above
  // the location bar's other children.
  void UpdateLayout();
  // Clears views focus and stored focus from the content.
  void ClearFocusFromContent();
  // Called once the compositor presents the first frame after `Show()`.
  void OnContentPresented(const gfx::PresentationFeedback& feedback);

  const raw_ref<LocationBarView> location_bar_view_;
  const raw_ref<OmniboxController> controller_;

  // Owned by `location_bar_view_`; removed and destroyed in the destructor.
  raw_ptr<OmniboxFullPopupWebUIContent> content_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      location_bar_observation_{this};

  // Invalidated on `Hide()` to drop a pending presentation callback.
  base::WeakPtrFactory<OmniboxPopupLocationBarHost> presentation_weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_
