// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"

class LocationBarView;
class OmniboxController;

// The `OmniboxPopupHost` that `LocationBarView` gives
// `OmniboxPopupViewFullWebUI` when
// `omnibox::ShouldHostUnboundedOmniboxInLocationBar()` is true, in place of the
// popup widget presenter. For now it shows nothing: every `OmniboxPopupHost`
// method is a no-op.
class OmniboxPopupLocationBarHost : public OmniboxPopupHost,
                                    public OmniboxPopupContentDelegate {
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
  void OnOmniboxFocused() override;
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

 private:
  const raw_ref<OmniboxController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_LOCATION_BAR_HOST_H_
