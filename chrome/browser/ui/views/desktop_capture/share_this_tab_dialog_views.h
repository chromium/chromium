// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_DIALOG_VIEWS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/share_this_tab_source_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

class ShareThisTabDialogViews;

// Dialog view used for ShareThisTabDialogViews.
class ShareThisTabDialogView : public views::DialogDelegateView {
  METADATA_HEADER(ShareThisTabDialogView, views::DialogDelegateView)

 public:
  ShareThisTabDialogView(const DesktopMediaPicker::Params& params,
                         ShareThisTabDialogViews* parent);
  ShareThisTabDialogView(const ShareThisTabDialogView&) = delete;
  ShareThisTabDialogView& operator=(const ShareThisTabDialogView&) = delete;
  ~ShareThisTabDialogView() override;

  void RecordUmaDismissal() const;

  // Called by parent (ShareThisTabDialogViews) when it's destroyed.
  void DetachParent();

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  bool ShouldShowWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;

 private:
  void SetupSourceView();
  void SetupAudioToggle();

  void Activate();

  bool ShouldAutoAccept() const;
  bool ShouldAutoReject() const;

  const base::WeakPtr<content::WebContents> web_contents_;
  const std::u16string app_name_;

  raw_ptr<ShareThisTabDialogViews> parent_;

  // Child view displaying a preview, icon and title for the tab being shared,
  // or a throbber while the dialog is not yet activated.
  raw_ptr<ShareThisTabSourceView> source_view_ = nullptr;

  raw_ptr<views::ToggleButton> audio_toggle_button_ = nullptr;

  // Timer for an initial delay during which the allow-button is disabled.
  base::OneShotTimer activation_timer_;

  // Auto-selection. Used only in tests.
  const std::string auto_select_tab_;        // Only tabs, by title.
  const std::string auto_select_source_;     // Any source by its title.
  const bool auto_accept_this_tab_capture_;  // Only for current-tab capture.
  const bool auto_reject_this_tab_capture_;  // Only for current-tab capture.

  // For recording dialog-duration UMA histograms.
  const base::TimeTicks dialog_open_time_;

  base::WeakPtrFactory<ShareThisTabDialogView> weak_factory_{this};
};

// Implementation of DesktopMediaPicker for the ShareThisTabDialogView.
class ShareThisTabDialogViews : public DesktopMediaPicker {
 public:
  ShareThisTabDialogViews();
  ShareThisTabDialogViews(const ShareThisTabDialogViews&) = delete;
  ShareThisTabDialogViews& operator=(const ShareThisTabDialogViews&) = delete;
  ~ShareThisTabDialogViews() override;

  void NotifyDialogResult(const content::DesktopMediaID& source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

 private:
  DoneCallback callback_;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When ShareThisTabDialogViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  raw_ptr<ShareThisTabDialogView> dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SHARE_THIS_TAB_DIALOG_VIEWS_H_
