// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_

#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_pane_view.h"
#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class TabbedPane;
class MdTextButton;
}  // namespace views

class DesktopMediaPickerViews;

BASE_DECLARE_FEATURE(kShareThisTabDialog);

// Dialog view used for DesktopMediaPickerViews.
//
// TODO(crbug.com/40637301): Consider renaming this class.
class DesktopMediaPickerDialogView : public views::DialogDelegateView,
                                     public views::TabbedPaneListener {
  METADATA_HEADER(DesktopMediaPickerDialogView, views::DialogDelegateView)

 public:
  // Used for UMA. Visible to this class's .cc file, but opaque beyond.
  enum class DialogType : int;
  DesktopMediaPickerDialogView(
      const DesktopMediaPicker::Params& params,
      DesktopMediaPickerViews* parent,
      std::vector<std::unique_ptr<DesktopMediaList>> source_lists);
  DesktopMediaPickerDialogView(const DesktopMediaPickerDialogView&) = delete;
  DesktopMediaPickerDialogView& operator=(const DesktopMediaPickerDialogView&) =
      delete;
  ~DesktopMediaPickerDialogView() override;

  void RecordUmaDismissal() const;

  // Called by parent (DesktopMediaPickerViews) when it's destroyed.
  void DetachParent();

  // Called by DesktopMediaListController.
  void OnSelectionChanged();
  void AcceptSource();
  void AcceptSpecificSource(const content::DesktopMediaID& source);
  void Reject();
  void OnSourceListLayoutChanged();
  void OnDelegatedSourceListDismissed();
  void OnCanReselectChanged(const DesktopMediaListController* controller);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  std::u16string GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool Accept() override;
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;
  void OnWidgetInitialized() override;

 private:
  friend class DesktopMediaPickerViewsTestApi;

  struct DisplaySurfaceCategory {
    DisplaySurfaceCategory(
        DesktopMediaList::Type type,
        std::unique_ptr<DesktopMediaListController> controller,
        bool audio_offered,
        bool audio_checked,
        bool supports_reselect_button);

    DisplaySurfaceCategory(DisplaySurfaceCategory&& other);

    ~DisplaySurfaceCategory();

    DesktopMediaList::Type type;
    std::unique_ptr<DesktopMediaListController> controller;
    bool audio_offered;  // Whether the audio-checkbox should be visible.
    bool audio_checked;  // Whether the audio-checkbox is checked.
    // Whether to show a button to allow re-selecting a choice within this
    // category. Primarily used if there is a separate selection surface that we
    // may need to re-open.
    bool supports_reselect_button;
    raw_ptr<DesktopMediaPaneView> pane = nullptr;
  };

  bool AudioSupported(DesktopMediaList::Type type);

  void ConfigureUIForNewPane(int index);
  void StoreAudioCheckboxState();
  void RemoveCurrentPaneUI();
  void MaybeCreateReselectButtonForPane(const DisplaySurfaceCategory& category);

  std::u16string GetLabelForAudioToggle(
      const DisplaySurfaceCategory& category) const;

  // Sets up the view for the pane based on the passed-in content_view and the
  // corresponding category object.
  std::unique_ptr<views::View> SetupPane(
      DesktopMediaList::Type type,
      std::unique_ptr<DesktopMediaListController> controller,
      bool audio_offered,
      bool audio_checked,
      bool supports_reselect_button,
      std::unique_ptr<views::View> content_view);

  void OnSourceTypeSwitched(int index);

  int GetSelectedTabIndex() const;

  const DesktopMediaListController* GetSelectedController() const;
  DesktopMediaListController* GetSelectedController();

  DesktopMediaList::Type GetSelectedSourceListType() const;
  bool IsAudioSharingApprovedByUser() const;

  // Records the number of tabs, windows and screens that were available
  // for the user to choose from when they eventually made their selection
  // of which tab/window/screen to capture.
  //
  // Note: The number of sources available can flactuate over time while
  // the media-picker is open. We only record the number at the end,
  // when the user either chooses what to capture, or chooses
  // not to capture anything.
  void RecordSourceCountsUma();

  // Helper for UMA-tracking of how often a user shares a discarded tab.
  void RecordTabDiscardedStatusUma(const content::DesktopMediaID& source);

  // Counts the number of sources of a given type.
  // * Returns nullopt if such sources are not offered to the user due to
  //   a configuration of the picker.
  // * Returns 0 if such sources were supposed to be offered to the user,
  //   but no such sources were available.
  std::optional<int> CountSourcesOfType(DesktopMediaList::Type type);

#if BUILDFLAG(IS_MAC)
  void OnPermissionUpdate(bool has_permission);
  void RecordPermissionInteractionUma() const;
#endif

  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_;
  const DesktopMediaPicker::Params::RequestSource request_source_;
  const std::u16string app_name_;
  const bool audio_requested_;
  const bool suppress_local_audio_playback_;  // Effective only if audio shared.
  const bool is_system_audio_offered_;
  const content::GlobalRenderFrameHostId capturer_global_id_;

  raw_ptr<DesktopMediaPickerViews> parent_;

  raw_ptr<views::Label> description_label_ = nullptr;

  raw_ptr<views::MdTextButton> reselect_button_ = nullptr;

  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;
  std::vector<DisplaySurfaceCategory> categories_;
  int previously_selected_category_ = 0;

  DialogType dialog_type_;

  std::optional<content::DesktopMediaID> accepted_source_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ScreenCapturePermissionChecker>
      screen_capture_permission_checker_;
  std::optional<bool> initial_permission_state_;
  bool permission_pane_was_shown_ = false;
#endif

  // For recording dialog-duration UMA histograms.
  const base::TimeTicks dialog_open_time_;

  base::WeakPtrFactory<DesktopMediaPickerDialogView> weak_factory_{this};
};

// Implementation of DesktopMediaPicker for Views.
//
// TODO(crbug.com/40637301): Rename this class.  Consider merging with
// DesktopMediaPickerController and naming the merged class just
// DesktopMediaPicker.
class DesktopMediaPickerViews : public DesktopMediaPicker {
 public:
  DesktopMediaPickerViews();
  DesktopMediaPickerViews(const DesktopMediaPickerViews&) = delete;
  DesktopMediaPickerViews& operator=(const DesktopMediaPickerViews&) = delete;
  ~DesktopMediaPickerViews() override;

  void NotifyDialogResult(const content::DesktopMediaID& source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

  DesktopMediaPickerDialogView* GetDialogViewForTesting() const {
    return dialog_;
  }

 private:
  friend class DesktopMediaPickerViewsTestApi;

  DoneCallback callback_;

  Params::RequestSource request_source_;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When DesktopMediaPickerViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  raw_ptr<DesktopMediaPickerDialogView> dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
