// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_DIALOG_H_

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

enum class AccessCodeCastDialogMode {
  // Dialog opened from the browser. This will match the styling of other
  // browser dialogs (i.e. Cast dialog), and will be positioned in the center of
  // the window and overlapping top-chrome.
  kBrowserStandard = 0,
  // Dialog is shown system modal, and matches the style for ChromeOS system
  // dialogs.
  kSystem = 1,
};

class AccessCodeCastDialog : public ui::WebDialogDelegate,
                             public views::WidgetObserver {
 public:
  AccessCodeCastDialog(
      const media_router::CastModeSet& cast_mode_set,
      std::unique_ptr<media_router::MediaRouteStarter> media_route_starter);
  ~AccessCodeCastDialog() override;
  AccessCodeCastDialog(const AccessCodeCastDialog&) = delete;
  AccessCodeCastDialog& operator=(const AccessCodeCastDialog&) = delete;

  static void Show(
      const media_router::CastModeSet& cast_mode_set,
      std::unique_ptr<media_router::MediaRouteStarter> media_route_starter,
      AccessCodeCastDialogOpenLocation open_location,
      AccessCodeCastDialogMode dialog_mode =
          AccessCodeCastDialogMode::kBrowserStandard);

  // Show the access code dialog box for desktop mirroring.
  static void ShowForDesktopMirroring(
      AccessCodeCastDialogOpenLocation open_location);

  void CloseDialogWidget();

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Blocks the widget automatically closing
  // when focus changes. This is to counter flaky tests.
  static void ShouldBlockWidgetActivationChangedForTest(bool should_block) {
    CHECK_IS_TEST();
    block_widget_activation_changed_for_test_ = should_block;
  }

  base::WeakPtr<AccessCodeCastDialog> GetWeakPtr();

 protected:
  // Creates default params for showing AccessCodeCastDialog
  virtual views::Widget::InitParams CreateParams(
      AccessCodeCastDialogMode dialog_mode);

 private:
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // Displays the dialog
  void ShowWebDialog(AccessCodeCastDialogMode dialog_mode);

  gfx::NativeView GetParentView();

  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  raw_ptr<content::WebUI> webui_ = nullptr;
  // Cast modes that should be attempted.
  const media_router::CastModeSet cast_mode_set_;

  // Important to note that this is a temporary placeholder - as soon as the
  // dialog is shown the pointer held by media_route_starter_ will be
  // moved to |AccessCodeCastUI|, so no |AccessCodeCastDialog| code should make
  // use of the media_route_starter_ pointer after c'tor.
  std::unique_ptr<media_router::MediaRouteStarter> media_route_starter_;

  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_;
  const raw_ptr<Profile> context_;
  base::Time dialog_creation_timestamp_;
  bool closing_dialog_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  static bool block_widget_activation_changed_for_test_;

  base::WeakPtrFactory<AccessCodeCastDialog> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_DIALOG_H_
